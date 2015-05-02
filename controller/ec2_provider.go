package main

import (
	"encoding/base64"
	"fmt"
	"io"
	"log"
	"net/http"
	"strings"
	"time"

	"github.com/awslabs/aws-sdk-go/aws"
	"github.com/awslabs/aws-sdk-go/service/ec2"
)

type EC2Provider struct {
	credential  AWSCredential
	instanceIds []*string

	instanceNum  int
	instanceType string
}

func NewEC2Provider(credential AWSCredential) *EC2Provider {
	provider := new(EC2Provider)
	provider.credential = credential
	provider.instanceNum = 4
	provider.instanceType = "c4.8xlarge"
	return provider
}

func (provider *EC2Provider) RenderDebugHTML(w io.Writer) {
	conn := ec2.New(&aws.Config{
		Region:      "us-west-1",
		Credentials: &provider.credential,
	})

	hist, _ := conn.DescribeSpotPriceHistory(&ec2.DescribeSpotPriceHistoryInput{
		ProductDescriptions: []*string{newString("Linux/UNIX")},
		InstanceTypes:       []*string{&provider.instanceType},
	})

	dataText := []string{`["Time", "Price"]`}
	for _, entry := range hist.SpotPriceHistory {
		dataText = append(dataText,
			fmt.Sprintf("[new Date(\"%s\"),%s]", *entry.Timestamp, *entry.SpotPrice))
	}
	dataTextEsc := "[" + strings.Join(dataText, ",") + "]"

	fmt.Fprintf(w,
		`
		<div id="chart_ec2provider" style="width:600px; height:300px"></div>
		<script>
		google.load("visualization", "1", {packages:["corechart"]});
		google.setOnLoadCallback(draw_ec2provider_chart);
		function draw_ec2provider_chart() {
			var data = google.visualization.arrayToDataTable(%s);
			var options = {};
			new google.visualization.ScatterChart(
				document.getElementById("chart_ec2provider")).draw(data, options);
		}
		</script>
		`, dataTextEsc)

	for _, entry := range hist.SpotPriceHistory {
		fmt.Fprintf(w, "<h2>SPH</h2>")
		fmt.Fprintf(w, "AZ: %s\n", *entry.AvailabilityZone)
		fmt.Fprintf(w, "ProdDesc: %s\n", *entry.ProductDescription)
		fmt.Fprintf(w, "Type: %s\n", *entry.InstanceType)
		fmt.Fprintf(w, "Price: %s\n", *entry.SpotPrice)
		fmt.Fprintf(w, "Time: %s\n", *entry.Timestamp)
	}
}

func (provider *EC2Provider) SafeToString() string {
	return fmt.Sprintf("EC2Provider{%s × %d}",
		provider.instanceType, provider.instanceNum)
}

func (provider *EC2Provider) Prepare() []string {
	const n = 4

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

	userData := base64.StdEncoding.EncodeToString([]byte(bootScript))

	resp, err := conn.RunInstances(&ec2.RunInstancesInput{
		ImageID:          &imageId,
		MaxCount:         aws.Long(n),
		MinCount:         aws.Long(n),
		InstanceType:     &provider.instanceType,
		UserData:         &userData,
		SecurityGroupIDs: []*string{sgId},
	})
	if err != nil {
		fmt.Println("EC2 launch error", err)
		return []string{}
	}
	for _, instance := range resp.Instances {
		provider.instanceIds = append(
			provider.instanceIds, instance.InstanceID)
	}

	// Wait until the instance become running
	for {
		log.Println("Pinging status")
		resp, err := conn.DescribeInstanceStatus(&ec2.DescribeInstanceStatusInput{
			InstanceIDs: provider.instanceIds,
		})
		if err != nil {
			log.Println("EC2 status check error", err)
			time.Sleep(5 * time.Second)
			continue
		}
		if len(resp.InstanceStatuses) < n {
			log.Println("Number of statuses is too few")
			time.Sleep(5 * time.Second)
			continue
		}
		allRunning := true
		for _, status := range resp.InstanceStatuses {
			allRunning = allRunning && (*status.InstanceState.Name == "running")
		}
		if allRunning {
			break
		}
		time.Sleep(5 * time.Second)
	}

	urls := make([]string, 0)
	for _, instanceId := range provider.instanceIds {
		respDI, _ := conn.DescribeInstances(&ec2.DescribeInstancesInput{
			InstanceIDs: []*string{instanceId},
		})
		ipAddress := respDI.Reservations[0].Instances[0].PublicIPAddress
		if ipAddress == nil {
			panic("IP address unknown")
		}

		url := fmt.Sprintf("http://%s:8000", *ipAddress)
		blockUntilAvailable(url, 5*time.Second)

		urls = append(urls, url)
	}
	return urls
}

func (provider *EC2Provider) Discard() {
	conn := ec2.New(&aws.Config{
		Region:      "us-west-1",
		Credentials: &provider.credential,
	})

	_, err := conn.TerminateInstances(&ec2.TerminateInstancesInput{
		InstanceIDs: provider.instanceIds,
	})
	if err != nil {
		fmt.Println("EC2 instance termination failed. You might need to kill it yourself.", err)
	}

	// Wait real termination to ensure killing sg.
	provider.setSecurityGroup(false)
}

func (provider *EC2Provider) CalcBill() (string, float64) {
	const pricePerHourPerInst = 1.856

	pricePerHour := pricePerHourPerInst * float64(provider.instanceNum)
	return fmt.Sprintf("EC2 on-demand instance (%s) * %d",
		provider.instanceType, provider.instanceNum), pricePerHour
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
		GroupName:   &sgName,
		Description: newString("For workers in https://github.com/xanxys/pentatope"),
	})
	if err != nil {
		panic(err)
	}

	_, err = conn.AuthorizeSecurityGroupIngress(&ec2.AuthorizeSecurityGroupIngressInput{
		GroupID:    respSg.GroupID,
		IPProtocol: newString("tcp"),
		CIDRIP:     newString("0.0.0.0/0"),
		FromPort:   newInt64(8000),
		ToPort:     newInt64(8000),
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

func blockUntilAvailable(url string, interval time.Duration) {
	for {
		log.Println("Pinging", url, "for RPC availability")
		httpResp, _ := http.Post(url,
			"application/x-protobuf", strings.NewReader("PING"))
		if httpResp != nil && httpResp.StatusCode == 400 {
			break
		}
		time.Sleep(interval)
	}
	log.Println(url, "is now accepting request")
}
