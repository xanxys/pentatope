package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"math/rand"
	"net/http"
	"os"
	"os/exec"
	"path"
	"strings"
	"sync"
	"time"

	"code.google.com/p/gogoprotobuf/proto"
)

import pentatope "./pentatope"

/*
Provider is an abstract entity that:
* runs pentatope intance (in docker) somewhere
* makes the instance available via HTTP RPC
* supports clean construction/deconstruction operation
* estimates time and money costs involved

As use of Provider might invove real money, we need detailed invariance.
Namely, they should follow these:

* until .prepare is called, no cost should occur (external connection is ok)
* caller must .discard once they .prepare, even in rare events
  (such as SIGTERM)
** However, when .prepare throws ProviderValidationError,
   .discard should not be called, and the provider should clean
   any state before throwing the exception.
*/
type Provider interface {
	// Printing function that does not output senstitive information
	// like secret keys.
	SafeToString() string

	Prepare() []string
	Discard()
	CalcBill() (string, float64)
}

// Ask user whether given billing plan is ok or not in CUI.
func askBillingPlan(providers []Provider) bool {
	fmt.Println("==================== Estimated Price ====================")
	total_price := 0.0
	for _, provider := range providers {
		name, price := provider.CalcBill()
		fmt.Printf("%s  %f USD\n", name, price)
		total_price += price
	}
	fmt.Println("---------------------------------------------------------")
	fmt.Printf("%f USD\n", total_price)

	// Get user confirmation before any possible expense.
	fmt.Print("Are you sure? [y/N]")
	os.Stdout.Sync()
	answer, _ := bufio.NewReader(os.Stdin).ReadString('\n')
	return strings.TrimSpace(answer) == "y"
}

type TaskShard struct {
	frameIndex  int
	frameConfig *pentatope.CameraConfig
}

// Return response when RPC is successful, otherwise return nil.
func doRenderRequest(serverUrl string, request *pentatope.RenderRequest) *pentatope.RenderResponse {
	requestRaw, err := proto.Marshal(request)
	respHttp, err := http.Post(serverUrl,
		"application/x-protobuf", bytes.NewReader(requestRaw))
	if err != nil {
		log.Println("Error when doing RPC", err)
		return nil
	}

	respRaw, err := ioutil.ReadAll(respHttp.Body)
	respHttp.Body.Close()

	resp := &pentatope.RenderResponse{}
	err = proto.Unmarshal(respRaw, resp)
	if err != nil {
		log.Println("Invalid proto received from worker", err)
		return nil
	}
	return resp
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

func (ctrl *WorkerCacheController) canUseCacheFor(serverUrl string) bool {
	ctrl.mutex.Lock()
	defer ctrl.mutex.Unlock()

	return ctrl.cached[serverUrl]
}

func (ctrl *WorkerCacheController) setCacheState(serverUrl string, canUseCache bool) {
	ctrl.mutex.Lock()
	defer ctrl.mutex.Unlock()

	ctrl.cached[serverUrl] = canUseCache
}

// Return true when shard rendering succeeds.
func renderShard(
	cacheCtrl *WorkerCacheController,
	wholeTask *pentatope.RenderMovieTask, shard *TaskShard,
	serverUrl string, encoder *MovieEncoder) bool {

	for {
		log.Println("Rendering", shard.frameIndex, "in", serverUrl)
		req := &pentatope.RenderRequest{
			Task: &pentatope.RenderTask{
				SamplePerPixel: wholeTask.SamplePerPixel,
				Scene:          wholeTask.Scene,
				Camera:         shard.frameConfig,
			},
			SceneId: &cacheCtrl.sceneId,
		}

		canUseCache := cacheCtrl.canUseCacheFor(serverUrl)

		if canUseCache {
			log.Printf("Try using cache (id=%d)\n", cacheCtrl.sceneId)
			req.Task.Scene = nil
		}

		resp := doRenderRequest(serverUrl, req)

		if *resp.Status == pentatope.RenderResponse_SUCCESS {
			cacheCtrl.setCacheState(serverUrl, true)

			encoder.addFrame(shard.frameIndex, resp.Output)
			log.Println("Shard", shard, "complete")
			return true
		} else if *resp.Status == pentatope.RenderResponse_SCENE_UNAVAILABLE {
			cacheCtrl.setCacheState(serverUrl, false)

			if canUseCache {
				log.Printf("Cache unavailable despite expectation; disabling cache")
			} else {
				log.Println("Worker incorrectly returning SCENE_UNAVAILABLE")
				return false
			}
		} else {
			log.Println("Error in worker", resp.ErrorMessage)
			return false
		}
	}
}

type MovieEncoder struct {
	framerate float32
	imageDir  string
}

func NewMovieEncoder(framerate float32) *MovieEncoder {
	// Prepare output directory.
	imageDir, err := ioutil.TempDir("", "penc")
	if err != nil {
		log.Panicln("Failed to create a temporary image directory", err)
	}
	return &MovieEncoder{
		framerate: framerate,
		imageDir:  imageDir,
	}
}

func (encoder *MovieEncoder) addFrame(frameIndex int, imageBlob []byte) {
	imagePath := path.Join(encoder.imageDir, fmt.Sprintf("frame-%06d.png", frameIndex))
	ioutil.WriteFile(imagePath, imageBlob, 0777)
}

func (encoder *MovieEncoder) encodeToMp4File(outputMp4File string) {
	cmd := exec.Command(
		"ffmpeg",
		"-y", // Allow overwrite
		"-framerate", fmt.Sprintf("%f", encoder.framerate),
		"-i", path.Join(encoder.imageDir, "frame-%06d.png"),
		"-pix_fmt", "yuv444p",
		"-crf", "18", // visually lossless
		"-c:v", "libx264",
		"-loglevel", "warning",
		"-r", fmt.Sprintf("%f", encoder.framerate),
		outputMp4File)
	err := cmd.Run()
	if err != nil {
		log.Println("Encoding failed with", err)
	}
}

func (encoder *MovieEncoder) clean() {
	os.RemoveAll(encoder.imageDir)
}

func loadRenderMovieTask(inputFile string) *pentatope.RenderMovieTask {
	// Generate RenderRequest from inputFile.
	taskRaw, err := ioutil.ReadFile(inputFile)
	if err != nil {
		log.Panicln("Aborting because", err)
	}
	task := &pentatope.RenderMovieTask{}
	err = proto.Unmarshal(taskRaw, task)
	if err != nil {
		log.Panicln("Input file is invalid as RenverMovieTask")
	}
	return task
}

func render(providers []Provider, task *pentatope.RenderMovieTask, outputMp4File string) {
	if len(task.Frames) == 0 {
		log.Println("One or more frames required")
		return
	}

	encoder := NewMovieEncoder(*task.Framerate)
	defer encoder.clean()

	cTask := make(chan *TaskShard, 1)
	cResult := make(chan bool, len(task.Frames))
	cFinishers := make([]chan bool, 0)
	cDiscardEnded := make(chan bool)

	cacheCtrl := NewCacheController()

	log.Println("Preparing providers in parallel")
	for _, provider := range providers {
		go func(provider Provider) {
			log.Println("Preparing provider", provider.SafeToString())
			urls := provider.Prepare()
			defer func() {
				provider.Discard()
				cDiscardEnded <- true
			}()
			log.Println("provider urls:", urls)

			cFeederDead := make(chan bool)
			for _, url := range urls {
				cFinisher := make(chan bool)
				cFinishers = append(cFinishers, cFinisher)
				go func(serverUrl string) {
					for {
						select {
						case shard := <-cTask:
							cResult <- renderShard(cacheCtrl, task, shard, serverUrl, encoder)
						case <-cFinisher:
							log.Println("Shutting down feeder for", serverUrl)
							cFeederDead <- true
							return
						}
					}
				}(url)
			}

			// Wait all feeder to die. (because we can't allow defer
			// to run until they finish)
			for range urls {
				<-cFeederDead
			}
			log.Println("Provider", provider.SafeToString(), "ended its role")
		}(provider)
	}

	log.Println("Feeding tasks")
	for ix, frameConfig := range task.Frames {
		log.Println("Queueing", ix)
		cTask <- &TaskShard{ix, frameConfig}
	}
	log.Println("Waiting all shards to finish")
	for range task.Frames {
		<-cResult
	}
	log.Println("Sending terminate requests")
	for _, cFinisher := range cFinishers {
		cFinisher <- true
	}

	// Encode
	log.Println("Converting to mp4", outputMp4File)
	encoder.encodeToMp4File(outputMp4File)
	log.Println("Encoding finished")

	log.Println("Waiting discard to finish")
	for range providers {
		<-cDiscardEnded
	}
}

// Return core * hout of the given rendering task.
func estimateTaskDifficulty(task *pentatope.RenderMovieTask) float64 {
	const samplePerCoreSec = 15000
	samples := uint64(len(task.Frames)) * uint64(*task.Width) * uint64(*task.Height) * uint64(*task.SamplePerPixel)
	coreHour := float64(samples) / samplePerCoreSec / 3600
	log.Printf("Difficulty estimator #samples=%d -> %.2f core * hour", samples, coreHour)
	return coreHour
}

// Try to instantiate all specified providers. Note that result could be empty.
func createProviders(
	debugFe *DebugFrontend,
	coreNeeded float64, duration float64,
	localFlag *bool, awsFlag *string, gceFlag *string) []Provider {

	var providers []Provider
	if *localFlag {
		providers = append(providers, new(LocalProvider))
	}
	if *awsFlag != "" {
		awsJson, err := ioutil.ReadFile(*awsFlag)
		if err != nil {
			log.Println("Ignoring AWS because credential file was not found.")
		} else {
			var credential AWSCredential
			json.Unmarshal(awsJson, &credential)
			if credential.AccessKey == "" || credential.SecretAccessKey == "" {
				fmt.Printf("Couldn't read AccessKey or SecretAccessKey from %s\n", *awsFlag)
			} else {
				ec2Prov := NewEC2Provider(credential)
				debugFe.RegisterModule(ec2Prov)
				providers = append(providers, ec2Prov)
			}
		}
	}
	if *gceFlag != "" {
		gceKey, err := ioutil.ReadFile(*gceFlag)
		if err != nil {
			log.Println("Ignoring GCE because credential key couldn't be read.")
		} else {
			providers = append(providers, NewGCEProvider(gceKey, coreNeeded, duration))
		}
	}
	return providers
}

func main() {
	rand.Seed(time.Now().UTC().UnixNano())

	debugFe := RunDebuggerFrontend()
	log.Printf("Debugger interface: http://localhost:%d/debug\n", debugFe.Port)

	// Resource providers.
	localFlag := flag.Bool("local", false, "Use this machine.")
	awsFlag := flag.String("aws", "", "Use Amazon Web Services with a json credential file.")
	gceFlag := flag.String("gce", "", "Use Google Compute Engine with a text file containing API key.")
	// I/O
	input := flag.String("input", "", "Input .pb file containing an animation.")
	outputMp4 := flag.String("output-mp4", "", "Encode the results as H264/mp4.")
	flag.Parse()

	const targetHour = 10.0 / 60

	task := loadRenderMovieTask(*input)
	difficulty := estimateTaskDifficulty(task)
	coreNeeded := difficulty / targetHour
	log.Printf("Estimated: %.1f cores necessary for %.1f hour target\n", coreNeeded, targetHour)

	providers := createProviders(debugFe, coreNeeded, targetHour, localFlag, awsFlag, gceFlag)
	if len(providers) == 0 {
		log.Println("You need at least one usable provider.")
		os.Exit(1)
	}

	if !askBillingPlan(providers) {
		os.Exit(0)
	}

	render(providers, task, *outputMp4)
}
