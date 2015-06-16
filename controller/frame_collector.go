package main

import (
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path"
)

type FrameCollector struct {
	framerate float32
	imageDir  string

	blobs map[int][]byte
}

func NewFrameCollector(framerate float32) *FrameCollector {
	// Prepare output directory.
	imageDir, err := ioutil.TempDir("", "penc")
	if err != nil {
		log.Panicln("Failed to create a temporary image directory", err)
	}
	return &FrameCollector{
		framerate: framerate,
		imageDir:  imageDir,
		blobs:     make(map[int][]byte),
	}
}

func (collector *FrameCollector) AddFrame(frameIndex int, imageBlob []byte) {
	collector.blobs[frameIndex] = imageBlob
	imagePath := path.Join(collector.imageDir, fmt.Sprintf("frame-%06d.png", frameIndex))
	ioutil.WriteFile(imagePath, imageBlob, 0777)
}

func (collector *FrameCollector) RetrieveFrames() [][]byte {
	frames := make([][]byte, 0)
	for _, blob := range collector.blobs {
		frames = append(frames, blob)
	}
	if len(frames) != len(collector.blobs) {
		log.Panic("Trying to retrieve all frames from incomplete frame collection")
	}
	return frames
}

func (collector *FrameCollector) EncodeToMp4File(outputMp4File string) {
	cmd := exec.Command(
		"ffmpeg",
		"-y", // Allow overwrite
		"-framerate", fmt.Sprintf("%f", collector.framerate),
		"-i", path.Join(collector.imageDir, "frame-%06d.png"),
		"-pix_fmt", "yuv444p",
		"-crf", "18", // visually lossless
		"-c:v", "libx264",
		"-loglevel", "warning",
		"-r", fmt.Sprintf("%f", collector.framerate),
		outputMp4File)
	err := cmd.Run()
	if err != nil {
		log.Println("Encoding failed with", err)
	}
}

func (collector *FrameCollector) Clean() {
	os.RemoveAll(collector.imageDir)
}
