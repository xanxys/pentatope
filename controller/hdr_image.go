package main

import (
	"bytes"
	"image"
	"image/color"
	"image/png"
	"log"
)

import pentatope "./pentatope"

type HdrImage struct {
	width  int
	height int

	// (y, x, channel)
	// Channels are in {R, G, B} order.
	values []float32

	mantissaPngBlob []byte
}

func DecodeImageTile(tile *pentatope.ImageTile) *HdrImage {
	mantissa, err := png.Decode(bytes.NewReader(tile.BlobPngMantissa))
	if err != nil {
		log.Panic(err)
	}

	width := mantissa.Bounds().Dx()
	height := mantissa.Bounds().Dy()
	values := make([]float32, width*height*3)
	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			r, g, b, _ := mantissa.At(x, y).RGBA()
			values[(y*width+x)*3+0] = float32(r)
			values[(y*width+x)*3+1] = float32(g)
			values[(y*width+x)*3+2] = float32(b)
		}
	}

	return &HdrImage{
		width:           width,
		height:          height,
		values:          values,
		mantissaPngBlob: tile.BlobPngMantissa,
	}
}

func (hdrImage *HdrImage) GetDebugPngBlob() []byte {
	ldrImage := image.NewRGBA(image.Rect(0, 0, hdrImage.width, hdrImage.height))
	for y := 0; y < hdrImage.height; y++ {
		for x := 0; x < hdrImage.width; x++ {
			r := hdrImage.values[(y*hdrImage.width+x)*3+0]
			g := hdrImage.values[(y*hdrImage.width+x)*3+1]
			b := hdrImage.values[(y*hdrImage.width+x)*3+2]

			ldrImage.Set(x, y, color.RGBA{
				R: uint8(r),
				G: uint8(g),
				B: uint8(b),
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
