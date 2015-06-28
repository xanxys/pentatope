package main

import (
	"fmt"
	"io/ioutil"
	"log"
	"math"
	"os"
	"os/exec"
	"path"
)

type FrameCollector struct {
	framerate float32

	frames map[int]*HdrImage
}

func NewFrameCollector(framerate float32) *FrameCollector {
	return &FrameCollector{
		framerate: framerate,
		frames:    make(map[int]*HdrImage),
	}
}

func (collector *FrameCollector) AddFrame(frameIndex int, image *HdrImage) {
	collector.frames[frameIndex] = image
}

func (collector *FrameCollector) RetrieveFrames() []*HdrImage {
	frames := make([]*HdrImage, 0)
	for ix := 0; ix < len(collector.frames); ix++ {
		frames = append(frames, collector.frames[ix])
	}
	return frames
}

// Tonemap the frames so that they will fit in [0, 255] range.
func Tonemap(framerate float32, frames []*HdrImage) []*HdrImage {
	dispGamma := 2.2
	ldrFrames := make([]*HdrImage, 0)
	for _, frame := range frames {
		// Calculate max_v
		max_v := float32(0.0)
		for _, v := range frame.Values {
			if v > max_v {
				max_v = v
			}
		}
		// Apply scaling
		values := make([]float32, 0)
		for _, v := range frame.Values {
			values = append(values, float32(math.Pow(float64(v/max_v), 1/dispGamma)*255))
		}
		ldrFrame := &HdrImage{
			Width:  frame.Width,
			Height: frame.Height,
			Values: values,
		}
		ldrFrames = append(ldrFrames, ldrFrame)
	}
	return ldrFrames
}

func (collector *FrameCollector) EncodeToMp4File(outputMp4File string) {
	// Prepare output directory.
	imageDir, err := ioutil.TempDir("", "penc")
	if err != nil {
		log.Panicln("Failed to create a temporary image directory", err)
	}
	defer os.RemoveAll(imageDir)

	for frameIndex, frame := range Tonemap(collector.framerate, collector.RetrieveFrames()) {
		imagePath := path.Join(imageDir, fmt.Sprintf("frame-%06d.png", frameIndex))
		ioutil.WriteFile(imagePath, frame.GetSaturatedU8Png(), 0777)
	}

	cmd := exec.Command(
		"ffmpeg",
		"-y", // Allow overwrite
		"-framerate", fmt.Sprintf("%f", collector.framerate),
		"-i", path.Join(imageDir, "frame-%06d.png"),
		"-pix_fmt", "yuv444p",
		"-crf", "18", // visually lossless
		"-c:v", "libx264",
		"-loglevel", "warning",
		"-r", fmt.Sprintf("%f", collector.framerate),
		outputMp4File)
	err = cmd.Run()
	if err != nil {
		log.Println("Encoding failed with", err)
	}
}

func (collector *FrameCollector) Clean() {
}
