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

func (provider *FakeProvider) Prepare() chan Rpc {
	log.Print("FakeProvider.Prepare")
	time.Sleep(time.Second) // Block for arbitrary short amount of time.
	urls := make(chan Rpc, 1)
	urls <- InProcessRpc{}
	return urls
}

func (provider *FakeProvider) NotifyUseless(server Rpc) {
	log.Print("FakeProvider.NotifyUseless: ", server)
}

func (provider *FakeProvider) Discard() {
	log.Print("FakeProvider.Discard")
}

func (provider *FakeProvider) CalcBill() (string, float64) {
	return "Infinitely powerful computer", 0
}
