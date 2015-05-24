package main

import (
	"log"
	"net/http"
	"strings"
	"time"
)

func BlockUntilAvailable(url string, interval time.Duration) {
	for {
		log.Println("Pinging", url, "for RPC availability")
		httpResp, _ := http.Post(url,
			"application/x-protobuf", strings.NewReader("PING"))
		if httpResp != nil && httpResp.StatusCode == 400 {
			break
		}
		time.Sleep(interval)
	}
	log.Println(url, "is now accepting request")
}
