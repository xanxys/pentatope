package main

import (
	"bytes"
	"io/ioutil"
	"log"
	"net/http"

	"code.google.com/p/gogoprotobuf/proto"
)

import pentatope "./pentatope"

// Virtual RPC endpoint that can be remote or in-process.
type Rpc interface {
	// Return response when RPC is successful, otherwise return nil.
	// When the result is nil, it is guranteed that error is non-nil.
	DoRenderRequest(request *pentatope.RenderRequest) (*pentatope.RenderResponse, error)

	// Unique opaque id that each implementation can use.
	GetId() string
}

type HttpRpc struct {
	url string
}

func NewHttpRpc(url string) HttpRpc {
	return HttpRpc{
		url: url,
	}
}

func (server HttpRpc) DoRenderRequest(request *pentatope.RenderRequest) (*pentatope.RenderResponse, error) {
	requestRaw, err := proto.Marshal(request)
	respHttp, err := http.Post(server.url,
		"application/x-protobuf", bytes.NewReader(requestRaw))
	if err != nil {
		log.Println("Error when doing RPC", err)
		return nil, err
	}

	respRaw, err := ioutil.ReadAll(respHttp.Body)
	respHttp.Body.Close()

	resp := &pentatope.RenderResponse{}
	err = proto.Unmarshal(respRaw, resp)
	if err != nil {
		log.Println("Invalid proto received from worker", err)
		return nil, err
	}
	return resp, nil
}

func (server HttpRpc) GetId() string {
	return server.url
}

type InProcessRpc struct {
}

func (rpc InProcessRpc) DoRenderRequest(request *pentatope.RenderRequest) (*pentatope.RenderResponse, error) {
	return nil, nil
}

func (rpc InProcessRpc) GetId() string {
	return "In-memory RPC"
}
