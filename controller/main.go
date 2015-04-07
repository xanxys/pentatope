package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"math/rand"
	"net/http"
	"os"
	"os/exec"
	"path"
	"strings"

	"code.google.com/p/gogoprotobuf/proto"
	"github.com/awslabs/aws-sdk-go/aws"
	"github.com/awslabs/aws-sdk-go/service/ec2"
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

type LocalProvider struct {
	containerId string
}

type EC2Provider struct {
}

func (provider *LocalProvider) Prepare() []string {
	container_name := fmt.Sprintf("pentatope_local_worker_%d", rand.Intn(1000))
	port := 20000 + rand.Intn(10000)
	cmd := exec.Command("sudo", "docker", "run",
		"--detach=true",
		"--name", container_name,
		"--publish", fmt.Sprintf("%d:80", port),
		"xanxys/pentatope-prod",
		"/root/pentatope/pentatope")

	var out bytes.Buffer
	cmd.Stdout = &out
	cmd.Run()
	provider.containerId = out.String()
	urls := make([]string, 1)
	urls[0] = fmt.Sprintf("http://localhost:%d/", port)
	return urls
}

func (provider *LocalProvider) Discard() {
	exec.Command("sudo", "docker", "rm", "-f", provider.containerId).Run()
}

func (provider *LocalProvider) CalcBill() (string, float64) {
	return "This machine", 0
}

func NewEC2Provider(credential AWSCredential) *EC2Provider {
	provider := new(EC2Provider)
	return provider
}

func (provider *EC2Provider) Prepare() []string {
	ec2.New(&aws.Config{Region: "us-west-1"})
	return nil
}

func (provider *EC2Provider) Discard() {
}

func (provider *EC2Provider) CalcBill() (string, float64) {
	const pricePerHour = 1.856
	const instanceType = "c4.8xlarge"
	/*# self.price_per_hour = 0.232
	  # self.instance_type = 'c4.xlarge'
	  # self.price_per_hour = 0.116
	  # self.instance_type = 'c4.large'*/
	return fmt.Sprintf("EC2 on-demand instance (%s) * 1", instanceType), pricePerHour
}

type AWSCredential struct {
	access_key        string
	secret_access_key string
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

func render(providers []Provider, inputFile string, outputMp4File string) {
	// Generate RenderRequest from inputFile.
	taskRaw, err := ioutil.ReadFile(inputFile)
	if err != nil {
		fmt.Println("Aborting because", err)
		return
	}
	task := &pentatope.RenderMovieTask{}
	err = proto.Unmarshal(taskRaw, task)
	if err != nil {
		fmt.Println("Input file is invalid as RenverMovieTask")
		return
	}
	if len(task.Frames) == 0 {
		fmt.Println("One or more frames required")
		return
	}

	// Prepare providers.
	for _, provider := range providers {
		provider.Prepare()
		defer provider.Discard()
	}
	// Prepare output directory.
	imageDir, err := ioutil.TempDir("", "penc")
	if err != nil {
		fmt.Println("Failed to create a temporary image directory", err)
		return
	}
	defer os.RemoveAll(imageDir)

	// Render frame-by-frame.
	for ix, frameConfig := range task.Frames {
		taskFrame := &pentatope.RenderTask{}
		taskFrame.SamplePerPixel = task.SamplePerPixel
		taskFrame.Scene = task.Scene
		taskFrame.Camera = frameConfig

		request := &pentatope.RenderRequest{}
		request.Task = taskFrame

		requestRaw, err := proto.Marshal(request)
		respHttp, err := http.Post("http://localhost/",
			"application/x-protobuf", bytes.NewReader(requestRaw))
		if err != nil {
			fmt.Println("Error reported when rendering frame", ix, err)
			continue
		}

		resp := &pentatope.RenderResponse{}
		respRaw, err := ioutil.ReadAll(respHttp.Body)
		respHttp.Body.Close()

		err = proto.Unmarshal(respRaw, resp)
		if err != nil {
			fmt.Println("Invalid proto received from worker", err)
			continue
		}
		if !*resp.IsOk {
			fmt.Println("Error in worker", resp.ErrorMessage)
			continue
		}

		imagePath := path.Join(imageDir, fmt.Sprintf("frame-%06d.png", ix))
		ioutil.WriteFile(imagePath, resp.Output, 0777)
	}

	// Encode
	fmt.Println("Done! (TODO: encode with ffmpeg)")
}

func main() {
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
			fmt.Println("Ignoring AWS because credential file was not found.")
		} else {
			var credential AWSCredential
			json.Unmarshal(awsJson, &credential)
			providers = append(providers, NewEC2Provider(credential))
		}
	}
	if len(providers) == 0 {
		fmt.Println("You need at least one usable provider.")
		os.Exit(1)
	}

	if !askBillingPlan(providers) {
		os.Exit(0)
	}

	render(providers, *input, *outputMp4)
}
