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
import render_task_pb2 as proto


def generate_fractal_noise(size, deterministic=False):
    """
    * size: tuple of int, length in each dimension
    Generate perlin-like fractal noise.
    """
    ndim = len(size)

    n_octaves = int(math.ceil(math.log(max(size), 2)))
    internal_size1 = 2 ** n_octaves
    assert(0 < max(size) <= internal_size1)

    if deterministic:
        np.random.seed(1)

    img = np.zeros((internal_size1,) * ndim)
    for i in range(n_octaves):
        octave = 2 ** i
        coeff = i + 1
        octave_size = (internal_size1 // octave,) * ndim
        img += coeff * scipy.ndimage.interpolation.zoom(
            np.random.rand(*octave_size),
            octave)

    return img[[slice(0, s) for s in size]]


def add_land(scene):
    """
    * scene: proto.RenderScene
    Add land objects in [-10, 10]^3 * [1, 5]
    """
    n = 10
    land_size = 20

    img = generate_fractal_noise([n, n, n], deterministic=True)
    # Normalize to [1, 2].
    v_min = np.min(img)
    v_max = np.max(img)
    img = (img - v_min) / (v_max - v_min) * 4 + 1.0
    # Generate OBBs.
    grid_size = land_size / n
    offset = -np.array([1, 1, 1, 0]) * land_size / 2
    for ix in range(n - 1):
        for iy in range(n - 1):
            for iz in range(n - 1):
                p_base_center = np.array([ix, iy, iz, 0]) * grid_size + offset
                height = img[ix, iy, iz]

                aabb_center = p_base_center + np.array([0, 0, 0, height / 2])
                aabb_size = np.array([grid_size, grid_size, grid_size, height])
                # Create an object as a new element in the scene.
                obj = scene.objects.add()
                geom = obj.geometry
                # Populate a geometry.
                geom.type = proto.ObjectGeometry.OBB
                obb = geom.Extensions[proto.OBBGeometry.geom]
                obb.local_to_world.rotation.extend(list(np.eye(4).flatten()))
                obb.local_to_world.translation.extend(list(aabb_center))
                obb.size.extend(list(aabb_size))
                # Populate Material.
                material = obj.material
                material.type = proto.ObjectMaterial.UNIFORM_LAMBERT
                lambert = material.Extensions[
                    proto.UniformLambertMaterialProto.material]
                lambert.reflectance.r = 1
                lambert.reflectance.g = 1
                lambert.reflectance.b = 1


def add_landscape(scene):
    """
    Add everything (trees, lands, lights) to scene.
    """
    add_land(scene)

    light = scene.lights.add()
    light.type = proto.SceneLight.POINT
    point_light = light.Extensions[proto.PointLightProto.light]
    point_light.translation.extend([0, 0, 0, 15])
    point_light.power.r = 1000
    point_light.power.g = 1000
    point_light.power.b = 1000


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="""
Generate a scene containing a fractal landscape and trees.""",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    args = parser.parse_args()

    scene = proto.RenderScene()
    add_land(scene)

    with open("scene.pb", "wb") as f_scene:
        f_scene.write(scene.SerializeToString())
