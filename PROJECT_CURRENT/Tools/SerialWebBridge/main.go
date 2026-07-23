// Command serial_web_bridge is a standalone (no install, no runtime deps)
// Serial<->Web bridge for the ECU_TestV1_EGT_DRY_START_PATCH firmware.
//
// Serves the exact same dashboard page the ECU's own WiFi web server serves
// (embedded from page.html, extracted verbatim from htmlPage()), but talks
// to the ECU over a Serial/USB-TTL link (GSU_DEBUG_UART1) instead of WiFi.
// Useful as a control channel that keeps working if the ECU's WiFi/SoftAP
// becomes unresponsive (e.g. under a brownout-adjacent voltage sag), since
// it never depends on the ESP32's WiFi radio at all.
//
// Build (Windows target, from any OS with Go installed):
//   GOOS=windows GOARCH=amd64 go build -o serial_web_bridge.exe .
//
// Run: double-click serial_web_bridge.exe, or from a terminal:
//   serial_web_bridge.exe [COM-port] [baud] [http-port]
// If COM-port is omitted, it is asked for interactively.
package main

import (
	"bufio"
	_ "embed"
	"fmt"
	"net/http"
	"net/url"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
)

//go:embed page.html
var pageHTML []byte

const (
	statusTimeout  = 1000 * time.Millisecond // how long to wait for the "apijson" reply line
	drainAfterCmd  = 150 * time.Millisecond  // time to drain a command's human-readable Serial.println output
	defaultBaud    = 115200
	defaultHTTPPrt = 8080
)

var (
	mu             sync.Mutex
	port           *comPort
	lastStatusJSON = []byte(`{"error":"no response from ECU yet"}`)
)

func askLine(prompt, def string) string {
	fmt.Print(prompt)
	if def != "" {
		fmt.Printf(" [%s]: ", def)
	} else {
		fmt.Print(": ")
	}
	reader := bufio.NewReader(os.Stdin)
	line, _ := reader.ReadString('\n')
	line = strings.TrimSpace(line)
	if line == "" {
		return def
	}
	return line
}

func main() {
	args := os.Args[1:]
	var portName string
	baud := defaultBaud
	httpPort := defaultHTTPPrt

	if len(args) >= 1 {
		portName = args[0]
	}
	if len(args) >= 2 {
		if b, err := strconv.Atoi(args[1]); err == nil {
			baud = b
		}
	}
	if len(args) >= 3 {
		if p, err := strconv.Atoi(args[2]); err == nil {
			httpPort = p
		}
	}

	fmt.Println("=== ECU Serial<->Web Bridge ===")
	if portName == "" {
		portName = askLine("Nhap cong COM (vd: COM5)", "")
		for portName == "" {
			portName = askLine("Cong COM khong duoc de trong, nhap lai (vd: COM5)", "")
		}
	}
	if len(args) < 2 {
		if s := askLine("Baud rate", strconv.Itoa(baud)); s != "" {
			if b, err := strconv.Atoi(s); err == nil {
				baud = b
			}
		}
	}
	if len(args) < 3 {
		if s := askLine("HTTP port (mo trinh duyet tai http://127.0.0.1:<port>/)", strconv.Itoa(httpPort)); s != "" {
			if p, err := strconv.Atoi(s); err == nil {
				httpPort = p
			}
		}
	}

	var err error
	port, err = openComPort(portName, uint32(baud))
	if err != nil {
		fmt.Printf("Loi mo cong %s: %v\n", portName, err)
		fmt.Println("Nhan Enter de thoat...")
		bufio.NewReader(os.Stdin).ReadString('\n')
		os.Exit(1)
	}
	defer port.Close()
	fmt.Printf("Da mo %s @ %d baud\n", portName, baud)

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.Write(pageHTML)
	})
	http.HandleFunc("/api", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.Write(pollStatus())
	})
	http.HandleFunc("/cmd", func(w http.ResponseWriter, r *http.Request) {
		c := r.URL.Query().Get("c")
		if c != "" {
			c2, _ := url.QueryUnescape(c)
			sendCommand(c2)
		}
		w.Header().Set("Content-Type", "text/plain")
		w.Write([]byte("OK"))
	})

	addr := fmt.Sprintf("127.0.0.1:%d", httpPort)
	fmt.Printf("Dashboard: http://%s/\n", addr)
	fmt.Println("Dong cua so nay de dung. (Ctrl+C)")
	if err := http.ListenAndServe(addr, nil); err != nil {
		fmt.Println("Loi HTTP server:", err)
		fmt.Println("Nhan Enter de thoat...")
		bufio.NewReader(os.Stdin).ReadString('\n')
	}
}

// pollStatus sends "apijson" and returns the JSON text the firmware prints back.
func pollStatus() []byte {
	mu.Lock()
	defer mu.Unlock()
	port.Purge()
	port.WriteLine("apijson")
	line, ok := port.ReadLine(statusTimeout)
	if ok && strings.HasPrefix(line, "{") {
		lastStatusJSON = []byte(line)
	}
	return lastStatusJSON
}

// sendCommand mirrors the web /cmd?c=... route: sends the raw command and
// drains its human-readable Serial.println() reply so it doesn't pollute
// the next status poll.
func sendCommand(cmd string) {
	mu.Lock()
	defer mu.Unlock()
	port.WriteLine(cmd)
	port.Drain(drainAfterCmd)
}
