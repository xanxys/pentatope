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

	blobs map[int][]byte
}

func NewFrameCollector(framerate float32) *FrameCollector {
	return &FrameCollector{
		framerate: framerate,
		blobs:     make(map[int][]byte),
	}
}

func (collector *FrameCollector) AddFrame(frameIndex int, imageBlob []byte) {
	collector.blobs[frameIndex] = imageBlob
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
	// Prepare output directory.
	imageDir, err := ioutil.TempDir("", "penc")
	if err != nil {
		log.Panicln("Failed to create a temporary image directory", err)
	}
	defer os.RemoveAll(imageDir)

	for frameIndex, frameBlob := range collector.RetrieveFrames() {
		imagePath := path.Join(imageDir, fmt.Sprintf("frame-%06d.png", frameIndex))
		ioutil.WriteFile(imagePath, frameBlob, 0777)
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
