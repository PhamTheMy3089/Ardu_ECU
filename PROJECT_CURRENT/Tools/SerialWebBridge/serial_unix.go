//go:build !windows

package main

// Minimal POSIX serial backend (Linux/macOS). The primary target of the Go
// build is Windows (serial_windows.go); this exists so the tool also builds and
// can be tested against a pty on Unix. It does NOT configure baud - on real
// hardware set it first with e.g. `stty -F /dev/ttyUSB0 115200 raw`, or just use
// the Python script (serial_web_bridge.py), which is the recommended path on
// Linux/macOS.

import (
	"os"
	"time"
)

type comPort struct {
	f *os.File
}

func openComPort(name string, baud uint32) (*comPort, error) {
	_ = baud // baud is configured externally (stty) on this backend
	f, err := os.OpenFile(name, os.O_RDWR, 0)
	if err != nil {
		return nil, err
	}
	return &comPort{f: f}, nil
}

func (p *comPort) Close()             { p.f.Close() }
func (p *comPort) WriteLine(s string) { p.f.Write([]byte(s + "\n")) }

func (p *comPort) Read(buf []byte) int {
	p.f.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
	n, _ := p.f.Read(buf)
	return n
}
