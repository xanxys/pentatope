package main

import (
	"fmt"
	"log"

	"golang.org/x/oauth2"
	"golang.org/x/oauth2/google"
	"google.golang.org/api/compute/v1"
)

type GCEProvider struct {
	keyJson     []byte
	instanceIds []*string

	instanceNum  int
	instanceType string
}

func NewGCEProvider(keyJson []byte) *GCEProvider {
	provider := new(GCEProvider)
	provider.keyJson = keyJson

	provider.instanceNum = 1
	provider.instanceType = "n1-standard-32"
	return provider
}

func (provider *GCEProvider) SafeToString() string {
	return fmt.Sprintf("GCEProvider{%s × %d}",
		provider.instanceType, provider.instanceNum)
}

func (provider *GCEProvider) Prepare() []string {
	const projectId = "pentatope-955"
	conf, err := google.JWTConfigFromJSON(provider.keyJson, "https://www.googleapis.com/auth/compute")
	if err != nil {
		log.Fatal(err)
	}

	client := conf.Client(oauth2.NoContext)
	service, err := compute.New(client)
	if err != nil {
		log.Fatal(err)
	}
	res, err := service.Images.List(projectId).Do()
	log.Printf("Got compute.Images.List, err: %#v, %v", res, err)
	return []string{}
}

func (provider *GCEProvider) Discard() {
}

func (provider *GCEProvider) CalcBill() (string, float64) {
	const pricePerHourPerInst = 1.856

	pricePerHour := pricePerHourPerInst * float64(provider.instanceNum)
	return fmt.Sprintf("EC2 on-demand instance (%s) * %d",
		provider.instanceType, provider.instanceNum), pricePerHour
}
