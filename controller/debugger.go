package main

import (
	"fmt"
	"log"
	"net"
	"net/http"
)

// Open debug HTTP server and return its port.
func RunDebuggerFrontend() int {
	ln, err := net.Listen("tcp", ":0")
	if err != nil {
		log.Fatal(err)
	}

	http.HandleFunc("/debug", httpHandler)
	go http.Serve(ln, nil)

	return ln.Addr().(*net.TCPAddr).Port
}

func httpHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Fprintf(w, "This is debug interface")
}
