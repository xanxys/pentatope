package main

import (
	"fmt"
	"log"
	"math/rand"
	"sync"
	"time"
)

import pentatope "./pentatope"

type TaskShard struct {
	frameIndex  int
	frameConfig *pentatope.CameraConfig
}

type WorkerCacheController struct {
	sceneId uint64

	mutex  sync.Mutex
	cached map[string]bool
}

func NewCacheController() *WorkerCacheController {
	return &WorkerCacheController{
		sceneId: uint64(rand.Int63()),
		cached:  make(map[string]bool),
	}
}

func (ctrl *WorkerCacheController) canUseCacheFor(server Rpc) bool {
	ctrl.mutex.Lock()
	defer ctrl.mutex.Unlock()

	return ctrl.cached[server.GetId()]
}

func (ctrl *WorkerCacheController) setCacheState(server Rpc, canUseCache bool) {
	ctrl.mutex.Lock()
	defer ctrl.mutex.Unlock()

	ctrl.cached[server.GetId()] = canUseCache
}

// Return nil when shard rendering failed.
func renderShard(
	cacheCtrl *WorkerCacheController,
	wholeTask *pentatope.RenderMovieTask, shard *TaskShard,
	server Rpc, collector *FrameCollector) error {

	for {
		log.Println("Rendering", shard.frameIndex, "in", server.GetId())
		req := &pentatope.RenderRequest{
			Task: &pentatope.RenderTask{
				SamplePerPixel: wholeTask.SamplePerPixel,
				Scene:          wholeTask.Scene,
				Camera:         shard.frameConfig,
			},
			SceneId: &cacheCtrl.sceneId,
		}

		canUseCache := cacheCtrl.canUseCacheFor(server)

		if canUseCache {
			log.Printf("Try using cache (id=%d)\n", cacheCtrl.sceneId)
			req.Task.Scene = nil
		}

		resp, err := server.DoRenderRequest(req)
		if err != nil {
			return err
		}

		if *resp.Status == pentatope.RenderResponse_SUCCESS {
			cacheCtrl.setCacheState(server, true)

			collector.AddFrameTile(shard.frameIndex, resp.OutputTile)
			log.Println("Shard", shard, "complete")
			return nil
		} else if *resp.Status == pentatope.RenderResponse_SCENE_UNAVAILABLE {
			cacheCtrl.setCacheState(server, false)

			if canUseCache {
				log.Printf("Cache unavailable despite expectation; disabling cache")
			} else {
				log.Println("Worker incorrectly returning SCENE_UNAVAILABLE")
				return fmt.Errorf("SCENE_UNAVAILABLE at server=%s", server.GetId())
			}
		} else {
			log.Println("Error in worker", resp.ErrorMessage)
			return fmt.Errorf("Error at server=%s, err=%s", server.GetId(), resp.ErrorMessage)
		}
	}
}

type WorkerPool struct {
	cTask  chan *TaskShard
	nTasks int

	cResult   chan bool
	collector *FrameCollector
	provider  Provider
}

func NewWorkerPool(provider Provider, task *pentatope.RenderMovieTask, collector *FrameCollector) *WorkerPool {
	const MAX_FAILURES = 3
	cTask := make(chan *TaskShard, 1)
	cResult := make(chan bool, len(task.Frames))

	cacheCtrl := NewCacheController()

	log.Println("Preparing provider", provider.SafeToString())
	cServices := provider.Prepare()
	go func() {
		servers := make(map[Rpc]bool) // server id -> isIdle
		failures := make(map[Rpc]int) // server id -> failure count
		for {
			select {
			case shard := <-cTask:
				var idleServer Rpc
				for server, isIdle := range servers {
					if isIdle {
						log.Println("An idle server found")
						idleServer = server
						break
					}
				}

				if idleServer != nil {
					go func() {
						servers[idleServer] = false
						err := renderShard(cacheCtrl, task, shard, idleServer, collector)
						if err == nil {
							cResult <- true
						} else {
							log.Println("Shard failed in server. Re-queueing the shard.")
							failures[idleServer]++
							if failures[idleServer] >= MAX_FAILURES {
								log.Printf("Removing %s since its failure count is %d\n", idleServer.GetId(), failures[idleServer])
								delete(servers, idleServer)
								delete(failures, idleServer)
							}
							cTask <- shard
						}
						servers[idleServer] = true
					}()
				} else {
					// All servers are busy now.
					time.Sleep(time.Second / 10)
					go func() {
						cTask <- shard
					}()
				}
			case service := <-cServices:
				log.Printf("Got new server: %s\n", service.GetId())
				servers[service] = true
				failures[service] = 0
			}
		}
	}()

	return &WorkerPool{
		cTask:     cTask,
		cResult:   cResult,
		provider:  provider,
		collector: collector,
		nTasks:    0,
	}
}

// Request to process a new task. Can block if there's
// too many unprocessed task.
func (pool *WorkerPool) AddShard(shard *TaskShard) {
	pool.nTasks++
	pool.cTask <- shard
}

func (pool *WorkerPool) WaitFinish() {
	for ix := 0; ix < pool.nTasks; ix++ {
		<-pool.cResult
	}
	log.Println("Sending terminate request")
	pool.provider.Discard()
}

func (pool *WorkerPool) WaitDiscard() {
}
