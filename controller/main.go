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

// Return true when shard rendering succeeds.
func renderShard(
	wholeTask *pentatope.RenderMovieTask, shard *TaskShard,
	serverUrl string, imageDir string) bool {

	log.Println("Rendering", shard.frameIndex, "in", serverUrl)
	taskFrame := &pentatope.RenderTask{
		SamplePerPixel: wholeTask.SamplePerPixel,
		Scene:          wholeTask.Scene,
		Camera:         shard.frameConfig,
	}

	request := &pentatope.RenderRequest{
		Task: taskFrame,
	}

	requestRaw, err := proto.Marshal(request)
	respHttp, err := http.Post(serverUrl,
		"application/x-protobuf", bytes.NewReader(requestRaw))
	if err != nil {
		log.Println("Error when rendering frame",
			shard.frameIndex, err)
		return false
	}

	resp := &pentatope.RenderResponse{}
	respRaw, err := ioutil.ReadAll(respHttp.Body)
	respHttp.Body.Close()

	err = proto.Unmarshal(respRaw, resp)
	if err != nil {
		log.Println("Invalid proto received from worker", err)
		return false
	}
	if !*resp.IsOk {
		log.Println("Error in worker", resp.ErrorMessage)
		return false
	}
	imagePath := path.Join(imageDir, fmt.Sprintf("frame-%06d.png", shard.frameIndex))
	ioutil.WriteFile(imagePath, resp.Output, 0777)
	log.Println("Shard", shard, "complete")
	return true
}

func render(providers []Provider, inputFile string, outputMp4File string) {
	// Generate RenderRequest from inputFile.
	taskRaw, err := ioutil.ReadFile(inputFile)
	if err != nil {
		log.Println("Aborting because", err)
		return
	}
	task := &pentatope.RenderMovieTask{}
	err = proto.Unmarshal(taskRaw, task)
	if err != nil {
		log.Println("Input file is invalid as RenverMovieTask")
		return
	}
	if len(task.Frames) == 0 {
		log.Println("One or more frames required")
		return
	}

	// Prepare output directory.
	imageDir, err := ioutil.TempDir("", "penc")
	if err != nil {
		log.Println("Failed to create a temporary image directory", err)
		return
	}
	defer os.RemoveAll(imageDir)

	cTask := make(chan *TaskShard)
	cResult := make(chan bool)
	cFinishers := make([]chan bool, 0)
	cDiscardEnded := make(chan bool)

	log.Println("Preparing providers in parallel")
	for _, provider := range providers {
		go func(provider Provider) {
			log.Println("Preparing provider", provider)
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
							cResult <- renderShard(task, shard, serverUrl, imageDir)
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
			log.Println("Provider", provider, "ended its role")
		}(provider)
	}

	cResultAggregate := make(chan bool)
	go func() {
		for range task.Frames {
			<-cResult
		}
		cResultAggregate <- true
	}()

	log.Println("Feeding tasks")
	for ix, frameConfig := range task.Frames {
		log.Println("Queueing", ix)
		cTask <- &TaskShard{ix, frameConfig}
	}
	log.Println("Waiting all shards to finish")
	<-cResultAggregate
	log.Println("Sending terminate requests")
	for _, cFinisher := range cFinishers {
		cFinisher <- true
	}

	// Encode
	log.Println("Converting to mp4", outputMp4File)
	cmd := exec.Command(
		"ffmpeg",
		"-y", // Allow overwrite
		"-framerate", fmt.Sprintf("%f", *task.Framerate),
		"-i", path.Join(imageDir, "frame-%06d.png"),
		"-pix_fmt", "yuv444p",
		"-crf", "18", // visually lossless
		"-c:v", "libx264",
		"-loglevel", "warning",
		"-r", fmt.Sprintf("%f", *task.Framerate),
		outputMp4File)
	err = cmd.Run()
	if err != nil {
		log.Println("Encoding failed with", err)
	}
	log.Println("Encoding finished")

	log.Println("Waiting discard to finish")
	for range providers {
		<-cDiscardEnded
	}
}

func main() {
	rand.Seed(time.Now().UTC().UnixNano())

	// Resource providers.
	localFlag := flag.Bool("local", false, "Use this machine.")
	awsFlag := flag.String("aws", "", "Use Amazon Web Services with a json credential file.")
	// I/O
	input := flag.String("input", "", "Input .pb file containing an animation.")
	outputMp4 := flag.String("output-mp4", "", "Encode the results as H264/mp4.")
	flag.Parse()

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
				providers = append(providers, NewEC2Provider(credential))
			}
		}
	}
	if len(providers) == 0 {
		log.Println("You need at least one usable provider.")
		os.Exit(1)
	}

	if !askBillingPlan(providers) {
		os.Exit(0)
	}

	render(providers, *input, *outputMp4)
}
