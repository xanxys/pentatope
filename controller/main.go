package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"

	//	"github.com/awslabs/aws-sdk-go/aws"
	//"github.com/awslabs/aws-sdk-go/service/ec2"
)

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
	server *exec.Cmd
}

type EC2Provider struct {
}

func (provider *LocalProvider) Prepare() []string {
	server := exec.Command("/root/pentatope/pentatope")
	server.Start()
	urls := make([]string, 1)
	urls[0] = "http://localhost"
	return urls
}

func (provider *LocalProvider) Discard() {
	provider.server.Process.Kill()
}

func (provider *LocalProvider) CalcBill() (string, float64) {
	return "This machine", 0
}

func NewEC2Provider(credential AWSCredential) *EC2Provider {
	provider := new(EC2Provider)
	return provider
}

func (provider *EC2Provider) Prepare() []string {
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

func main() {
	// Resource providers.
	local := flag.Bool("local", false, "Use this machine.")
	aws := flag.String("aws", "", "Use Amazon Web Services with a json credential file.")
	// I/O
	input := flag.String("input", "", "Input .pb file containing an animation.")
	output_mp4 := flag.String("output-mp4", "", "Encode the results as H264/mp4.")
	flag.Parse()

	var providers []Provider
	if *local {
		providers = append(providers, new(LocalProvider))
	}
	if *aws != "" {
		awsJson, err := ioutil.ReadFile(*aws)
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

	//
	fmt.Println(input, output_mp4)

	// fmt.Println("Hello World", *local, *aws, *input, *output_mp4, "FFMPEG:", is_ffmpeg_installed())
}
