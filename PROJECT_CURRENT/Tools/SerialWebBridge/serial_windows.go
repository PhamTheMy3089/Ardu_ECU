//go:build windows

package main

import (
	"fmt"
	"strings"
	"syscall"
	"time"
	"unsafe"
)

// Raw Win32 COM-port I/O via kernel32.dll only - deliberately avoids any
// third-party module so this builds and cross-compiles with nothing beyond
// the Go standard library (no `go mod download` / network access needed).

var (
	kernel32            = syscall.NewLazyDLL("kernel32.dll")
	procGetCommState    = kernel32.NewProc("GetCommState")
	procSetCommState    = kernel32.NewProc("SetCommState")
	procSetCommTimeouts = kernel32.NewProc("SetCommTimeouts")
	procPurgeComm       = kernel32.NewProc("PurgeComm")
)

const purgeRxClear = 0x0008

// dcb mirrors the Win32 DCB struct (see <winbase.h>). Go has no bitfields, so
// the packed flags DWORD is built by hand; field sizes/order match exactly
// (verified by the init() size check below), giving the same 28-byte layout.
type dcb struct {
	DCBlength  uint32
	BaudRate   uint32
	Flags      uint32 // fBinary(bit0)=1 required; fDtrControl/fRtsControl left at 0 (disabled) - see openComPort
	wReserved  uint16
	XonLim     uint16
	XoffLim    uint16
	ByteSize   byte
	Parity     byte
	StopBits   byte
	XonChar    byte
	XoffChar   byte
	ErrorChar  byte
	EofChar    byte
	EvtChar    byte
	wReserved1 uint16
}

type commTimeouts struct {
	ReadIntervalTimeout         uint32
	ReadTotalTimeoutMultiplier  uint32
	ReadTotalTimeoutConstant    uint32
	WriteTotalTimeoutMultiplier uint32
	WriteTotalTimeoutConstant   uint32
}

func init() {
	if unsafe.Sizeof(dcb{}) != 28 {
		panic(fmt.Sprintf("internal error: dcb struct size = %d, want 28 (Win32 DCB layout mismatch)", unsafe.Sizeof(dcb{})))
	}
}

type comPort struct {
	h syscall.Handle
}

func openComPort(name string, baud uint32) (*comPort, error) {
	full := name
	if !strings.HasPrefix(full, `\\.\`) {
		full = `\\.\` + full
	}
	pathPtr, err := syscall.UTF16PtrFromString(full)
	if err != nil {
		return nil, err
	}
	h, err := syscall.CreateFile(pathPtr,
		syscall.GENERIC_READ|syscall.GENERIC_WRITE,
		0, nil, syscall.OPEN_EXISTING, 0, 0)
	if err != nil {
		return nil, fmt.Errorf("CreateFile: %w", err)
	}

	var d dcb
	procGetCommState.Call(uintptr(h), uintptr(unsafe.Pointer(&d)))
	d.DCBlength = uint32(unsafe.Sizeof(d))
	d.BaudRate = baud
	d.Flags = 1 // fBinary=1 (required); DTR/RTS control bits left 0 (disabled) so
	// opening the port never asserts/toggles DTR or RTS - this link only ever
	// wires GND+TX+RX (see README), no reset/boot lines involved.
	d.ByteSize = 8
	d.Parity = 0 // NOPARITY
	d.StopBits = 0 // ONESTOPBIT
	r, _, e := procSetCommState.Call(uintptr(h), uintptr(unsafe.Pointer(&d)))
	if r == 0 {
		syscall.CloseHandle(h)
		return nil, fmt.Errorf("SetCommState: %w", e)
	}

	// ReadIntervalTimeout=MAXDWORD + ReadTotalTimeoutMultiplier=0 +
	// ReadTotalTimeoutConstant=N means: return immediately if data is already
	// there, otherwise wait up to N ms for at least something to arrive.
	ct := commTimeouts{ReadIntervalTimeout: 0xFFFFFFFF, ReadTotalTimeoutConstant: 50}
	r, _, e = procSetCommTimeouts.Call(uintptr(h), uintptr(unsafe.Pointer(&ct)))
	if r == 0 {
		syscall.CloseHandle(h)
		return nil, fmt.Errorf("SetCommTimeouts: %w", e)
	}

	return &comPort{h: h}, nil
}

func (p *comPort) Close() {
	syscall.CloseHandle(p.h)
}

func (p *comPort) Purge() {
	procPurgeComm.Call(uintptr(p.h), uintptr(purgeRxClear))
}

func (p *comPort) WriteLine(s string) {
	buf := []byte(s + "\n")
	var n uint32
	syscall.WriteFile(p.h, buf, &n, nil)
}

// ReadLine accumulates bytes (each ReadFile call blocks up to ~50ms per the
// timeouts set in openComPort) until a newline is seen or timeout elapses.
func (p *comPort) ReadLine(timeout time.Duration) (string, bool) {
	deadline := time.Now().Add(timeout)
	buf := make([]byte, 256)
	var acc []byte
	for time.Now().Before(deadline) {
		var n uint32
		err := syscall.ReadFile(p.h, buf, &n, nil)
		if err == nil && n > 0 {
			acc = append(acc, buf[:n]...)
			if i := indexByte(acc, '\n'); i >= 0 {
				return strings.TrimRight(string(acc[:i]), "\r"), true
			}
		}
	}
	return "", false
}

// Drain reads and discards whatever arrives for the given duration (used
// after sending a plain command, so its human-readable reply doesn't leak
// into the next status poll).
func (p *comPort) Drain(d time.Duration) {
	deadline := time.Now().Add(d)
	buf := make([]byte, 256)
	for time.Now().Before(deadline) {
		var n uint32
		syscall.ReadFile(p.h, buf, &n, nil)
	}
}

func indexByte(b []byte, c byte) int {
	for i, x := range b {
		if x == c {
			return i
		}
	}
	return -1
}
