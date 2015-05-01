package main

import (
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
)

type DebugFrontend struct {
	modules []DebugModule
	Port    int
}

type DebugModule interface {
	RenderDebugHTML(io.Writer)
}

func (fe *DebugFrontend) RegisterModule(debugMod DebugModule) {
	fe.modules = append(fe.modules, debugMod)
}

// Open debug HTTP server and return its handler.
func RunDebuggerFrontend() *DebugFrontend {
	ln, err := net.Listen("tcp", ":0")
	if err != nil {
		log.Fatal(err)
	}

	debugFe := DebugFrontend{}

	http.HandleFunc("/debug", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprintf(w, "<!DOCTYPE html>")
		fmt.Fprintf(w, "#Debug modules: %d", len(debugFe.modules))
		for _, mod := range debugFe.modules {
			fmt.Fprintf(w, "<hr/>")
			mod.RenderDebugHTML(w)
		}
	})
	go http.Serve(ln, nil)

	debugFe.Port = ln.Addr().(*net.TCPAddr).Port
	return &debugFe
}

func httpHandler(w http.ResponseWriter, r *http.Request) {

}
