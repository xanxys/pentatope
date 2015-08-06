package main

import (
	"fmt"
	"log"
	"time"
)

import pentatope "./pentatope"

// No-cache server that always returns red image.
type FakeRpc struct {
}

func (rpc FakeRpc) DoRenderRequest(request *pentatope.RenderRequest) (*pentatope.RenderResponse, error) {
	// If request is expecting cache, always return unavailable.
	if request.Task.Scene == nil && request.SceneId != nil {
		resp := pentatope.RenderResponse{
			Status: pentatope.RenderResponse_SCENE_UNAVAILABLE.Enum(),
		}
		return &resp, nil
	}
	// Proceed as if scene was rendered
	tile := EncodeImageTile(generateImage(
		int(*request.Task.Camera.SizeX),
		int(*request.Task.Camera.SizeY)))
	resp := pentatope.RenderResponse{
		Status:     pentatope.RenderResponse_SUCCESS.Enum(),
		OutputTile: tile,
	}
	return &resp, nil
}

func generateImage(width, height int) *HdrImage {
	image := HdrImage{
		Width:  width,
		Height: height,
		Values: make([]float32, width*height*3),
	}
	for y := 0; y < image.Height; y++ {
		for x := 0; x < image.Width; x++ {
			offset := 3 * (y*image.Width + x)
			image.Values[offset+0] = 255
			image.Values[offset+1] = 0
			image.Values[offset+2] = 0
		}
	}
	return &image
}

func (rpc FakeRpc) GetId() string {
	return "Fake RPC"
}

type FakeProvider struct {
}

func (provider *FakeProvider) SafeToString() string {
	return fmt.Sprintf("FakeProvider{}")
}

func (provider *FakeProvider) Prepare() chan Rpc {
	log.Print("FakeProvider.Prepare")
	time.Sleep(time.Second) // Block for arbitrary short amount of time.
	urls := make(chan Rpc, 1)
	urls <- FakeRpc{}
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
