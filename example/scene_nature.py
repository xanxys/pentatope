#!/bin/python2
from __future__ import print_function, division
import argparse
import sys
from google.protobuf import text_format
import numpy as np
import math
import os.path
import multiprocessing
import cv2
import scipy.ndimage
sys.path.append('build/proto')
from render_task_pb2 import *


def generate_fractal_noise(size):
    """
    * size: tuple of int, length in each dimension
    Generate perlin-like fractal noise.
    """
    ndim = len(size)

    n_octaves = int(math.ceil(math.log(max(size), 2)))
    internal_size1 = 2 ** n_octaves
    assert(0 < max(size) <= internal_size1)

    img = np.zeros((internal_size1,) * ndim)
    for i in range(n_octaves):
        octave = 2 ** i
        coeff = i + 1
        octave_size = (internal_size1 // octave,) * ndim
        img += coeff * scipy.ndimage.interpolation.zoom(
            np.random.rand(*octave_size),
            octave)

    return img[[slice(0, s) for s in size]]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="""
Generate a scene containing a fractal landscape and trees.""",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    args = parser.parse_args()

    img = generate_fractal_noise([100, 100])


    # Nomalize
    v_min = np.min(img)
    v_max = np.max(img)
    img = (img - v_min) * 255 / (v_max - v_min)
    cv2.imwrite("test.png", img)
