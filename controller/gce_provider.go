package main

import (
	"fmt"
	"log"
	"math/rand"
	"strings"
	"time"

	"golang.org/x/oauth2"
	"golang.org/x/oauth2/google"
	"google.golang.org/api/compute/v1"
)

type GCEProvider struct {
	keyJson       []byte
	instanceNames []string

	instanceNum  int
	instanceType string

	projectId string
	zone      string

	runId string
}

func NewGCEProvider(keyJson []byte) *GCEProvider {
	provider := new(GCEProvider)
	provider.keyJson = keyJson

	provider.instanceNum = 1
	provider.instanceType = "n1-standard-8"
	provider.zone = "us-central1-a"

	provider.projectId = "pentatope-955"
	provider.runId = fmt.Sprintf("%04d", rand.Int()%10000)
	return provider
}

func (provider *GCEProvider) SafeToString() string {
	return fmt.Sprintf("GCEProvider{%s × %d}",
		provider.instanceType, provider.instanceNum)
}

func (provider *GCEProvider) Prepare() []string {
	service := provider.getService()

	prefix := "https://www.googleapis.com/compute/v1/projects/" + provider.projectId
	imageURL := "https://www.googleapis.com/compute/v1/projects/ubuntu-os-cloud/global/images/ubuntu-1504-vivid-v20150422"

	startupScript := strings.Join(
		[]string{
			`#!/bin/bash`,
			`apt-get update`,
			`apt-get -y install docker.io`,
			`service docker start`,
			`docker pull docker.io/xanxys/pentatope-prod`,
			`docker run --publish 8000:80 docker.io/xanxys/pentatope-prod /root/pentatope/worker`,
		}, "\n")

	for ix := 0; ix < provider.instanceNum; ix++ {
		provider.instanceNames = append(provider.instanceNames,
			fmt.Sprintf("pentatope-worker-%s-%d", provider.runId, ix))
	}

	for _, name := range provider.instanceNames {
		instance := &compute.Instance{
			Name:        name,
			Description: "Exposes renderer as proto/HTTP service.",
			MachineType: prefix + "/zones/" + provider.zone + "/machineTypes/" + provider.instanceType,
			Disks: []*compute.AttachedDisk{
				{
					AutoDelete: true,
					Boot:       true,
					Type:       "PERSISTENT",
					InitializeParams: &compute.AttachedDiskInitializeParams{
						DiskName:    "my-root-pd",
						SourceImage: imageURL,
					},
				},
			},
			NetworkInterfaces: []*compute.NetworkInterface{
				&compute.NetworkInterface{
					AccessConfigs: []*compute.AccessConfig{
						&compute.AccessConfig{
							Type: "ONE_TO_ONE_NAT",
							Name: "External NAT",
						},
					},
					Network: prefix + "/global/networks/default",
				},
			},
			ServiceAccounts: []*compute.ServiceAccount{
				{
					Email: "default",
					Scopes: []string{
						compute.DevstorageFullControlScope,
						compute.ComputeScope,
					},
				},
			},
			Metadata: &compute.Metadata{
				Items: []*compute.MetadataItems{
					{
						Key:   "startup-script",
						Value: startupScript,
					},
				},
			},
		}

		op, err := service.Instances.Insert(provider.projectId, provider.zone, instance).Do()
		log.Printf("Got compute.Operation, err: %#v, %v", op, err)
	}

	// Wait until the instances to become running & responding
	urls := make([]string, 0)
	for _, name := range provider.instanceNames {
		for {
			log.Printf("Pinging status for %s\n", name)
			resp, _ := service.Instances.Get(provider.projectId, provider.zone, name).Do()
			if resp != nil && resp.Status == "RUNNING" && len(resp.NetworkInterfaces) > 0 {
				ip := resp.NetworkInterfaces[0].AccessConfigs[0].NatIP
				url := fmt.Sprintf("http://%s:8000", ip)
				BlockUntilAvailable(url, 5*time.Second)
				urls = append(urls, url)
				break
			}
			time.Sleep(5 * time.Second)
		}
	}

	return urls
}

func (provider *GCEProvider) Discard() {
	service := provider.getService()

	for _, name := range provider.instanceNames {
		service.Instances.Delete(provider.projectId, provider.zone, name).Do()
	}
}

func (provider *GCEProvider) CalcBill() (string, float64) {
	const pricePerHourPerInst = 1.856

	pricePerHour := pricePerHourPerInst * float64(provider.instanceNum)
	return fmt.Sprintf("GCE instance (%s) * %d",
		provider.instanceType, provider.instanceNum), pricePerHour
}

func (provider *GCEProvider) getService() *compute.Service {
	conf, err := google.JWTConfigFromJSON(provider.keyJson,
		"https://www.googleapis.com/auth/cloud-platform",
		"https://www.googleapis.com/auth/compute")
	if err != nil {
		log.Fatal(err)
	}

	client := conf.Client(oauth2.NoContext)
	service, err := compute.New(client)
	if err != nil {
		log.Fatal(err)
	}
	return service
}
