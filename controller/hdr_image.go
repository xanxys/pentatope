package main

import (
	"bytes"
	"image"
	"image/color"
	"image/png"
	"log"
	"math"
)

import pentatope "./pentatope"

type HdrImage struct {
	Width  int
	Height int

	// (y, x, channel)
	// Channels are in {R, G, B} order.
	Values []float32
}

func DecodeImageTile(tile *pentatope.ImageTile) *HdrImage {
	mantissa, err := png.Decode(bytes.NewReader(tile.BlobPngMantissa))
	if err != nil {
		log.Panic(err)
	}
	exponent, err := png.Decode(bytes.NewReader(tile.BlobPngExponent))
	if err != nil {
		log.Panic(err)
	}

	width := mantissa.Bounds().Dx()
	height := mantissa.Bounds().Dy()
	values := make([]float32, width*height*3)
	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			rm, gm, bm, _ := mantissa.At(x, y).RGBA()
			re, ge, be, _ := exponent.At(x, y).RGBA()
			values[(y*width+x)*3+0] = combineFloat(uint8(rm), uint8(re))
			values[(y*width+x)*3+1] = combineFloat(uint8(gm), uint8(ge))
			values[(y*width+x)*3+2] = combineFloat(uint8(bm), uint8(be))
		}
	}

	return &HdrImage{
		Width:  width,
		Height: height,
		Values: values,
	}
}

func combineFloat(mantissa, exponent uint8) float32 {
	return float32((float64(mantissa)/256.0 + 1.0) * math.Pow(2, float64(exponent)-127))
}

func (hdrImage *HdrImage) GetSaturatedU8Png() []byte {
	ldrImage := image.NewRGBA(image.Rect(0, 0, hdrImage.Width, hdrImage.Height))
	for y := 0; y < hdrImage.Height; y++ {
		for x := 0; x < hdrImage.Width; x++ {
			r := hdrImage.Values[(y*hdrImage.Width+x)*3+0]
			g := hdrImage.Values[(y*hdrImage.Width+x)*3+1]
			b := hdrImage.Values[(y*hdrImage.Width+x)*3+2]

			ldrImage.Set(x, y, color.RGBA{
				R: uint8(math.Max(0, math.Min(255, float64(r)))),
				G: uint8(math.Max(0, math.Min(255, float64(g)))),
				B: uint8(math.Max(0, math.Min(255, float64(b)))),
				A: 255,
			})
		}
	}

	pngBuffer := &bytes.Buffer{}
	err := png.Encode(pngBuffer, ldrImage)
	if err != nil {
		log.Panic(err)
	}
	return pngBuffer.Bytes()
}
