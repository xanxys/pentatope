package main

import (
	"bytes"
	"fmt"
	"log"
	"math/rand"
	"os/exec"
	"strings"
	"time"
)

type LocalProvider struct {
	containerId string
}

func (provider *LocalProvider) SafeToString() string {
	return fmt.Sprintf("LocalProvider{%s}", provider.containerId)
}

func (provider *LocalProvider) Prepare() chan Rpc {
	container_name := fmt.Sprintf("pentatope_local_worker_%d", rand.Intn(1000))
	port := 20000 + rand.Intn(10000)
	cmd := exec.Command("sudo", "docker", "run",
		"--detach=true",
		"--name", container_name,
		"--publish", fmt.Sprintf("%d:80", port),
		WorkerContainerName,
		WorkerPathInContainer)

	var out bytes.Buffer
	cmd.Stdout = &out
	cmd.Run()
	provider.containerId = strings.TrimSpace(out.String())
	log.Printf("LocalProvider docker container id: %s", provider.containerId)
	url := fmt.Sprintf("http://localhost:%d/", port)
	BlockUntilAvailable(url, time.Second)
	service := NewHttpRpc(url)
	services := make(chan Rpc, 1)
	services <- service
	return services
}

func (provider *LocalProvider) NotifyUseless(server Rpc) {
}

func (provider *LocalProvider) Discard() {
	err := exec.Command("sudo", "docker", "rm", "-f", provider.containerId).Run()
	if err != nil {
		fmt.Println("Container clean up failed. You may need to clean up docker container manually ", err)
	}
}

func (provider *LocalProvider) CalcBill() (string, float64) {
	return "This machine", 0
}
