package main

import (
	"fmt"
	"log"
	"math/rand"
	"time"
)

import pentatope "./pentatope"

// No-cache server that always returns red image.
type FakeRpc struct {
	rng        *rand.Rand
	sceneCache map[uint64]*pentatope.RenderScene
}

func newFakeRpc() FakeRpc {
	return FakeRpc{
		rng:        rand.New(rand.NewSource(1)),
		sceneCache: make(map[uint64]*pentatope.RenderScene),
	}
}

func (rpc FakeRpc) DoRenderRequest(request *pentatope.RenderRequest) (*pentatope.RenderResponse, error) {
	rpc.invalidateRandomCache()

	if request.Task.Scene == nil && request.SceneId != nil {
		_, exist := rpc.sceneCache[*request.SceneId]
		if exist {
			// no need to do anything special; we don't eve need scene!
		} else {
			return sceneUnavailable()
		}
	}

	// Cache given scene if possible.
	if request.Task.Scene != nil && request.SceneId != nil {
		rpc.sceneCache[*request.SceneId] = request.Task.Scene
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

// Maybe randomly invalidates random scene cache to simulate realistic memory.
func (rpc FakeRpc) invalidateRandomCache() {
	const keepProb = 0.25
	newSceneCache := make(map[uint64]*pentatope.RenderScene)

	for id, scene := range rpc.sceneCache {
		if rpc.rng.Float32() > keepProb {
			newSceneCache[id] = scene
		} else {
			log.Printf("FakeRpc: invalidating cache(%d)", id)
		}
	}
	rpc.sceneCache = newSceneCache
}

func sceneUnavailable() (*pentatope.RenderResponse, error) {
	resp := pentatope.RenderResponse{
		Status: pentatope.RenderResponse_SCENE_UNAVAILABLE.Enum(),
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
	urls <- newFakeRpc()
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
