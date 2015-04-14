package main

import (
	"bytes"
	"encoding/base64"
	"fmt"
	"math/rand"
	"net/http"
	"os/exec"
	"strings"
	"time"

	"github.com/awslabs/aws-sdk-go/aws"
	"github.com/awslabs/aws-sdk-go/service/ec2"
)

type LocalProvider struct {
	containerId string
}

type EC2Provider struct {
	credential AWSCredential
	instanceId string
}

func (provider *LocalProvider) Prepare() []string {
	container_name := fmt.Sprintf("pentatope_local_worker_%d", rand.Intn(1000))
	port := 20000 + rand.Intn(10000)
	cmd := exec.Command("sudo", "docker", "run",
		"--detach=true",
		"--name", container_name,
		"--publish", fmt.Sprintf("%d:80", port),
		"xanxys/pentatope-prod",
		"/root/pentatope/worker")

	var out bytes.Buffer
	cmd.Stdout = &out
	cmd.Run()
	provider.containerId = strings.TrimSpace(out.String())
	urls := make([]string, 1)
	urls[0] = fmt.Sprintf("http://localhost:%d/", port)
	return urls
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

func NewEC2Provider(credential AWSCredential) *EC2Provider {
	provider := new(EC2Provider)
	provider.credential = credential
	return provider
}

func (provider *EC2Provider) Prepare() []string {
	conn := ec2.New(&aws.Config{
		Region:      "us-west-1",
		Credentials: &provider.credential,
	})
	
	sgId := provider.setSecurityGroup(true)

	bootScript := strings.Join(
		[]string{
			`#cloud-config`,
			`repo_upgrade: all`,
			`packages:`,
			` - docker`,
			`runcmd:`,
			` - ["/etc/init.d/docker", "start"]`,
			` - ["docker", "pull", "xanxys/pentatope-prod"]`,
			` - ["docker", "run", "--detach=true", "--publish", "8000:80", "xanxys/pentatope-prod", "/root/pentatope/worker"]`,
		}, "\n")

	imageId := "ami-d114f295" // us-west-1
	// instType := "c4.8xlarge"
	instType := "c4.2xlarge"

	userData := base64.StdEncoding.EncodeToString([]byte(bootScript))

	resp, err := conn.RunInstances(&ec2.RunInstancesInput{
		ImageID:      &imageId,
		MaxCount:     aws.Long(1),
		MinCount:     aws.Long(1),
		InstanceType: &instType,
		UserData:     &userData,
		SecurityGroupIDs: []*string {sgId},
	})
	if err != nil {
		fmt.Println("EC2 launch error", err)
		return []string{}
	}
	provider.instanceId = *resp.Instances[0].InstanceID

	// Wait until the instance become running
	for {
		fmt.Println("Pinging status")
		resp, err := conn.DescribeInstanceStatus(&ec2.DescribeInstanceStatusInput{
			InstanceIDs: []*string{&provider.instanceId},
		})
		if err != nil {
			fmt.Println("EC2 status check error", err)
			time.Sleep(5 * time.Second)
			continue
		}
		if len(resp.InstanceStatuses) == 0 {
			fmt.Println("No result for status check")
			time.Sleep(5 * time.Second)
			continue
		}
		if *resp.InstanceStatuses[0].InstanceState.Name == "running" {
			break
		}

		time.Sleep(5 * time.Second)
	}

	respDI, _ := conn.DescribeInstances(&ec2.DescribeInstancesInput{
		InstanceIDs: []*string{&provider.instanceId},
	})
	ipAddress := respDI.Reservations[0].Instances[0].PublicIPAddress
	if ipAddress == nil {
		panic("IP address unknown")
	}

	url := fmt.Sprintf("http://%s:8000", *ipAddress)
	for {
		fmt.Println("Pinging status for RPC availability")
		httpResp, err := http.Post(url,
			"application/x-protobuf", strings.NewReader("PING"))
		if err != nil {
			fmt.Println(err)
		}
		if httpResp != nil && httpResp.StatusCode == 400 {
			break
		}
		time.Sleep(5 * time.Second)
	}
	return []string{url}
}

func (provider *EC2Provider) Discard() {
	conn := ec2.New(&aws.Config{
		Region:      "us-west-1",
		Credentials: &provider.credential,
	})

	_, err := conn.TerminateInstances(&ec2.TerminateInstancesInput{
		InstanceIDs: []*string{&provider.instanceId},
	})
	if err != nil {
		fmt.Println("EC2 instance termination failed. You might need to kill it yourself.", err)
	}

	// Wait real termination to ensure killing sg.
	provider.setSecurityGroup(false)
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

// Return security group id or nil.
func (provider *EC2Provider) setSecurityGroup(exist bool) *string {
	sgName := "pentatope_sg"
	conn := ec2.New(&aws.Config{
		Region:      "us-west-1",
		Credentials: &provider.credential,
	})

	// Destroy current SG if exists, and confirm deletion.
	// We call DeleteSecurityGroup many times because instances
	// not yet terminated would prevent SG deletion.
	for {
		conn.DeleteSecurityGroup(&ec2.DeleteSecurityGroupInput{
			GroupName: &sgName,
		})
		resp, err := conn.DescribeSecurityGroups(&ec2.DescribeSecurityGroupsInput{})
		if err == nil {
			n := 0
			for _, sg := range resp.SecurityGroups {
				if *sg.GroupName == sgName {
					n += 1
				}
			}
			if n == 0 {
				break
			}
		}
		time.Sleep(5 * time.Second)
	}

	// Newly create one if requeted.
	if !exist {
		return nil
	}
	respSg, err := conn.CreateSecurityGroup(&ec2.CreateSecurityGroupInput{
		GroupName: &sgName,
		Description: newString("For workers in https://github.com/xanxys/pentatope"),
	})
	if err != nil {
		panic(err)
	}

	_, err = conn.AuthorizeSecurityGroupIngress(&ec2.AuthorizeSecurityGroupIngressInput{
		GroupID: respSg.GroupID,
		IPProtocol: newString("tcp"),
		CIDRIP: newString("0.0.0.0/0"),
		FromPort: newInt64(8000),
		ToPort: newInt64(8000),
	})
	if err != nil {
		panic(err)
	}
	return respSg.GroupID
}

func newString(s string) *string {
	return &s
}

func newInt64(x int64) *int64 {
	return &x
}

type AWSCredential struct {
	AccessKey       string `json:"access_key"`
	SecretAccessKey string `json:"secret_access_key"`
}

func (credential *AWSCredential) Credentials() (*aws.Credentials, error) {
	return &aws.Credentials{
		AccessKeyID:     credential.AccessKey,
		SecretAccessKey: credential.SecretAccessKey,
	}, nil
}
