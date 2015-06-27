package main

import ()

import pentatope "./pentatope"

type HdrImage struct {
	width  int
	height int

	// (y, x, channel)
	values []float32

	mantissaPngBlob []byte
}

func DecodeImageTile(tile *pentatope.ImageTile) *HdrImage {
	return &HdrImage{
		mantissaPngBlob: tile.BlobPngMantissa,
	}
}

func (image *HdrImage) GetDebugPngBlob() []byte {
	return image.mantissaPngBlob
}
