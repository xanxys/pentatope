package main

import (
	"fmt"
	"io/ioutil"
	"log"
	"math"
	"os"
	"os/exec"
	"path"
	"sort"
)

import pentatope "./pentatope"

type FrameCollector struct {
	framerate float32
	frames    map[int]*pentatope.ImageTile
}

func NewFrameCollector(framerate float32) *FrameCollector {
	return &FrameCollector{
		framerate: framerate,
		frames:    make(map[int]*pentatope.ImageTile),
	}
}

func (collector *FrameCollector) AddFrameTile(
	frameIndex int, image *pentatope.ImageTile) {
	collector.frames[frameIndex] = image
}

func (collector *FrameCollector) RetrieveFrameTiles() []*pentatope.ImageTile {
	frames := make([]*pentatope.ImageTile, 0)
	for ix := 0; ix < len(collector.frames); ix++ {
		frames = append(frames, collector.frames[ix])
	}
	return frames
}

// Tonemap the frames so that they will fit in [0, 255] range.
func Tonemap(framerate float32, frameTiles []*pentatope.ImageTile) []*pentatope.ImageTile {
	// pupillary reflex takes about 250ms to complete.
	// http://www.faa.gov/data_research/research/med_humanfacs/oamtechreports/1960s/media/AM65-25.pdf
	reflexLatency := 0.25
	blendRatio := 1 - math.Pow(0.1, 1.0/(float64(framerate)*reflexLatency))

	dispGamma := 2.2
	ldrFrameTiles := make([]*pentatope.ImageTile, 0)
	prev_maxv := -1.0
	for _, frameTile := range frameTiles {
		frame := DecodeImageTile(frameTile)
		// Calculate 99%-ile max_v
		vs := make([]float64, len(frame.Values))
		for ix, v := range frame.Values {
			vs[ix] = float64(v)
		}
		sort.Sort(sort.Float64Slice(vs))
		max_v := vs[int(float64(len(vs))*0.99)]
		// Smoothly blend.
		smooth_max_v := 0.0
		if prev_maxv < 0 {
			smooth_max_v = max_v
		} else {
			smooth_max_v = math.Exp((1-blendRatio)*math.Log(prev_maxv) + blendRatio*math.Log(max_v))
		}
		prev_maxv = smooth_max_v

		// Apply scaling
		values := make([]float32, len(frame.Values))
		for ix, v := range frame.Values {
			values[ix] = float32(math.Pow(float64(v)/smooth_max_v, 1/dispGamma) * 255)
		}
		ldrFrame := &HdrImage{
			Width:  frame.Width,
			Height: frame.Height,
			Values: values,
		}
		ldrFrameTiles = append(ldrFrameTiles, EncodeImageTile(ldrFrame))
	}
	return ldrFrameTiles
}

func (collector *FrameCollector) EncodeToMp4File(outputMp4File string) {
	// Prepare output directory.
	imageDir, err := ioutil.TempDir("", "penc")
	if err != nil {
		log.Panicln("Failed to create a temporary image directory", err)
	}
	defer os.RemoveAll(imageDir)

	for frameIndex, frameTile := range Tonemap(collector.framerate, collector.RetrieveFrameTiles()) {
		imagePath := path.Join(imageDir, fmt.Sprintf("frame-%06d.png", frameIndex))
		ioutil.WriteFile(
			imagePath, DecodeImageTile(frameTile).GetSaturatedU8Png(), 0777)
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
