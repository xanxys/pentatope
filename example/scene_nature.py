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
    n = 30
    land_size = 20

    img = generate_fractal_noise([n, n, n], deterministic=True)
    # Normalize to [1, 2].
    v_min = np.min(img)
    v_max = np.max(img)
    img_w = (img - v_min) / (v_max - v_min) * 4 + 1.0

    img_x, img_y, img_z = land_size * (np.mgrid[0:n, 0:n, 0:n] / n - 0.5)
    img_pos = np.transpose([img_x, img_y, img_z, img_w], [1, 2, 3, 0])

    # tetrahedron = base triangle + the top vertex
    # tri0, tri1, tri2 (CCW), top (anti-normal side of the tri)
    tetrahedrons = [
        # base tris on Z=0 plane
        [(0, 0, 0), (1, 0, 0), (1, 1, 0), (1, 0, 1)],
        [(0, 0, 0), (1, 1, 0), (0, 1, 0), (0, 1, 1)],
        # base tris on Z=1 plane
        [(1, 0, 1), (0, 1, 1), (0, 0, 1), (0, 0, 0)],
        [(1, 0, 1), (1, 1, 1), (0, 1, 1), (1, 1, 0)],
        # middle
        [(1, 0, 1), (0, 0, 0), (1, 1, 0), (0, 1, 1)]
    ]

    # Create 4D membrane by tetrahedrons.
    for ix in xrange(n - 1):
        for iy in xrange(n - 1):
            for iz in xrange(n - 1):
                for tetra in tetrahedrons:
                    vs = [img_pos[ix + dx, iy + dy, iz + dz] for (dx, dy, dz) in tetra]

                    # Create an object as a new element in the scene.
                    obj = scene.objects.add()
                    geom = obj.geometry
                    # Populate a geometry.
                    geom.type = proto.ObjectGeometry.TETRAHEDRON
                    th = geom.Extensions[proto.TetrahedronGeometry.geom]
                    for (ref_vertex, val_vertex) in zip([th.vertex0, th.vertex1, th.vertex2, th.vertex3], vs):
                        ref_vertex.x = val_vertex[0]
                        ref_vertex.y = val_vertex[1]
                        ref_vertex.z = val_vertex[2]
                        ref_vertex.w = val_vertex[3]
                    # Populate Material.
                    material = obj.material
                    material.type = proto.ObjectMaterial.UNIFORM_LAMBERT
                    lambert = material.Extensions[
                        proto.UniformLambertMaterialProto.material]
                    lambert.reflectance.r = 0.3
                    lambert.reflectance.g = 0.25
                    lambert.reflectance.b = 0.2


def set_landscape(scene):
    """
    Set everything (trees, lands, lights) to scene.
    """
    scene.background_radiance.r = 1e-3
    scene.background_radiance.g = 1e-3
    scene.background_radiance.b = 1.2e-3

    add_land(scene)

    light = scene.lights.add()
    light.type = proto.SceneLight.POINT
    point_light = light.Extensions[proto.PointLightProto.light]
    point_light.translation.extend([0, 0, 0, 15])
    point_light.power.r = 1000
    point_light.power.g = 1000
    point_light.power.b = 1200


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
