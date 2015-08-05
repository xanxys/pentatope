package main

import (
	"fmt"
	"log"
	"time"
)

type FakeProvider struct {
}

func (provider *FakeProvider) SafeToString() string {
	return fmt.Sprintf("FakeProvider{}")
}

func (provider *FakeProvider) Prepare() chan string {
	log.Print("FakeProvider.Prepare")
	time.Sleep(time.Second) // Block for arbitrary short amount of time.
	// TODO: replace string with ServiceConnection, and return fake conn.
	urls := make(chan string, 1)
	return urls
}

func (provider *FakeProvider) NotifyUseless(server string) {
	log.Print("FakeProvider.NotifyUseless: ", server)
}

func (provider *FakeProvider) Discard() {
	log.Print("FakeProvider.Discard")
}

func (provider *FakeProvider) CalcBill() (string, float64) {
	return "Infinitely powerful computer", 0
}
