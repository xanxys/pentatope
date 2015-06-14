package main

import (
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path"
)

// This is currently more like a frame colelctor.
type MovieEncoder struct {
	framerate float32
	imageDir  string

	blobs map[int][]byte
}

func NewMovieEncoder(framerate float32) *MovieEncoder {
	// Prepare output directory.
	imageDir, err := ioutil.TempDir("", "penc")
	if err != nil {
		log.Panicln("Failed to create a temporary image directory", err)
	}
	return &MovieEncoder{
		framerate: framerate,
		imageDir:  imageDir,
	}
}

func (encoder *MovieEncoder) AddFrame(frameIndex int, imageBlob []byte) {
	encoder.blobs[frameIndex] = imageBlob
	imagePath := path.Join(encoder.imageDir, fmt.Sprintf("frame-%06d.png", frameIndex))
	ioutil.WriteFile(imagePath, imageBlob, 0777)
}

func (encoder *MovieEncoder) RetrieveFrames() [][]byte {
	frames := make([][]byte, 0)
	for _, blob := range encoder.blobs {
		frames = append(frames, blob)
	}
	if len(frames) != len(encoder.blobs) {
		log.Panic("Trying to retrieve all frames from incomplete frame collection")
	}
	return frames
}

func (encoder *MovieEncoder) EncodeToMp4File(outputMp4File string) {
	cmd := exec.Command(
		"ffmpeg",
		"-y", // Allow overwrite
		"-framerate", fmt.Sprintf("%f", encoder.framerate),
		"-i", path.Join(encoder.imageDir, "frame-%06d.png"),
		"-pix_fmt", "yuv444p",
		"-crf", "18", // visually lossless
		"-c:v", "libx264",
		"-loglevel", "warning",
		"-r", fmt.Sprintf("%f", encoder.framerate),
		outputMp4File)
	err := cmd.Run()
	if err != nil {
		log.Println("Encoding failed with", err)
	}
}

func (encoder *MovieEncoder) Clean() {
	os.RemoveAll(encoder.imageDir)
}
