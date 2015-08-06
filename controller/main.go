package main

import (
	"bufio"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"math/rand"
	"os"
	"strings"
	"time"

	"code.google.com/p/gogoprotobuf/proto"
)

import pentatope "./pentatope"

/*
Provider is an abstract entity that:
* runs pentatope instance (in docker) somewhere
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

	// Return a channel where usable rpc endpoints appear.
	Prepare() chan Rpc

	// Notify useless server that should be killed.
	NotifyUseless(service Rpc)

	Discard()
	CalcBill() (string, float64)
}

// Ask user whether given billing plan is ok or not in CUI.
func askBillingPlan(provider Provider) bool {
	fmt.Println("==================== Estimated Price ====================")
	total_price := 0.0
	name, price := provider.CalcBill()
	fmt.Printf("%s  %f USD\n", name, price)
	total_price += price
	fmt.Println("---------------------------------------------------------")
	fmt.Printf("%f USD\n", total_price)

	// Get user confirmation before any possible expense.
	fmt.Print("Are you sure? [y/N]")
	os.Stdout.Sync()
	answer, _ := bufio.NewReader(os.Stdin).ReadString('\n')
	return strings.TrimSpace(answer) == "y"
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

func render(provider Provider, task *pentatope.RenderMovieTask, outputMp4File string) {
	if len(task.Frames) == 0 {
		log.Println("One or more frames required")
		return
	}

	collector := NewFrameCollector(*task.Framerate)
	defer collector.Clean()

	pool := NewWorkerPool(provider, task, collector)

	log.Println("Feeding tasks")
	for ix, frameConfig := range task.Frames {
		log.Println("Queueing", ix)
		pool.AddShard(&TaskShard{ix, frameConfig})
	}
	log.Println("Waiting all shards to finish")
	pool.WaitFinish()

	// Encode
	log.Println("Converting to mp4", outputMp4File)
	collector.EncodeToMp4File(outputMp4File)
	log.Println("Encoding finished")

	log.Println("Waiting discard to finish")
	pool.WaitDiscard()
}

// Return core * hout of the given rendering task.
func estimateTaskDifficulty(task *pentatope.RenderMovieTask) float64 {
	const samplePerCoreSec = 15000
	samples := uint64(len(task.Frames)) * uint64(*task.Width) * uint64(*task.Height) * uint64(*task.SamplePerPixel)
	coreHour := float64(samples) / samplePerCoreSec / 3600
	log.Printf("Difficulty estimator #samples=%d -> %.2f core * hour", samples, coreHour)
	return coreHour
}

// Try to specified provider. Note that result could be empty.
func createprovider(
	debugFe *DebugFrontend,
	coreNeeded float64, duration float64,
	localFlag *bool, gceFlag *string, fakeFlag *bool) Provider {

	nProvider := 0
	if *localFlag {
		nProvider++
	}
	if *gceFlag != "" {
		nProvider++
	}
	if *fakeFlag {
		nProvider++
	}
	if nProvider != 1 {
		log.Println("You must specify one and only one provider.")
		return nil
	}

	if *localFlag {
		return new(LocalProvider)
	} else if *gceFlag != "" {
		gceKey, err := ioutil.ReadFile(*gceFlag)
		if err != nil {
			log.Println("Ignoring GCE because credential key couldn't be read.")
			return nil
		}
		return NewGCEProvider(gceKey, coreNeeded, duration)
	} else if *fakeFlag {
		return new(FakeProvider)
	}
	return nil
}

func main() {
	rand.Seed(time.Now().UTC().UnixNano())

	debugFe := RunDebuggerFrontend()
	log.Printf("Debugger interface: http://localhost:%d/debug\n", debugFe.Port)

	// Resource provider.
	localFlag := flag.Bool("local", false, "Use this machine.")
	gceFlag := flag.String("gce", "", "Use Google Compute Engine with a text file containing API key.")
	fakeFlag := flag.Bool("fake", false, "Use internal fake provider that ignores input scene. Useful for testing controller.")
	// I/O
	input := flag.String("input", "", "Input .pb file containing an animation.")
	outputMp4 := flag.String("output-mp4", "", "Encode the results as H264/mp4.")
	flag.Parse()

	const targetHour = 10.0 / 60

	task := loadRenderMovieTask(*input)
	difficulty := estimateTaskDifficulty(task)
	coreNeeded := difficulty / targetHour
	log.Printf("Estimated: %.1f cores necessary for %.1f hour target\n", coreNeeded, targetHour)

	provider := createprovider(debugFe, coreNeeded, targetHour, localFlag, gceFlag, fakeFlag)
	if provider == nil {
		log.Println("You need at one usable provider.")
		os.Exit(1)
	}

	if !askBillingPlan(provider) {
		os.Exit(0)
	}

	render(provider, task, *outputMp4)
}
