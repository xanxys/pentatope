#!/bin/python2
from __future__ import print_function, division
import argparse
import math
import numpy as np
import render_task_pb2 as proto
import scipy.ndimage
import scipy.linalg as la


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
                parity = (ix + iy + iz) % 2
                for tetra in tetrahedrons:
                    # Flip x-axis in odd-parity cubes to close the gap.
                    # See https://github.com/xanxys/pentatope/issues/30
                    vs = [img_pos[ix + (dx ^ parity), iy + dy, iz + dz]
                          for (dx, dy, dz) in tetra]

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

    # Plant trees.
    for i in xrange(5000):
        origin = img_pos[tuple(np.random.randint(0, n, 3))]
        for obj in create_tree(origin):
            scene.objects.add().CopyFrom(obj)


def create_tree(origin, up=np.array([0, 0, 0, 1])):
    """
    Instantiate a tree-like something using L-system.
    """
    objects = []

    def add_segment(p0, p1):
        # just create basis using randomized algorithm
        # We create OBB with
        # size = (l, t, t, t)
        # and want to calculate a rotation matrix m,
        # such that np.dot(m, (1, 0, 0, 0)) == n
        l = la.norm(p1 - p0)
        t = l * 0.1
        n = (p1 - p0) / l
        size = np.array([l, t, t, t])

        rot_local_to_world = np.zeros([4, 4])
        rot_local_to_world[:, 0] = n
        for axis in range(1, 4):
            e = np.random.randn(4)
            # Make e orthogonal to all existing axes.
            for axis_to_remove in range(0, axis):
                a = rot_local_to_world[:, axis_to_remove]
                e -= a * np.dot(a, e)
            e /= la.norm(e)
            rot_local_to_world[:, axis] = e
        if la.det(rot_local_to_world) < 0:
            rot_local_to_world[:, 0] *= -1

        if abs(la.det(rot_local_to_world) - 1) > 1e-6:
            print("det=", la.det(rot_local_to_world))
            print(rot_local_to_world)
        assert(abs(la.det(rot_local_to_world) - 1) <= 1e-6)

        # Emit object.
        sobj = proto.SceneObject()
        geom = sobj.geometry
        geom.type = proto.ObjectGeometry.OBB
        obb = geom.Extensions[proto.OBBGeometry.geom]
        obb.local_to_world.rotation.extend(list(rot_local_to_world.flatten()))
        obb.local_to_world.translation.extend(list((p0 + p1) / 2))
        obb.size.extend(list(size))

        mat = sobj.material
        mat.type = proto.ObjectMaterial.UNIFORM_LAMBERT
        lambert = mat.Extensions[
            proto.UniformLambertMaterialProto.material]
        lambert.reflectance.r = 0.8
        lambert.reflectance.g = 0.3
        lambert.reflectance.b = 0.3

        objects.append(sobj)

    def extend(base_pos, base_dir, segment_len):
        if segment_len < 0.3:
            return
        new_base = base_pos + base_dir * segment_len
        add_segment(base_pos, new_base)

        # Create two symmetric children whose center is slightly
        # different from base_dir.
        new_dir = sample_random(base_dir, 0.2)
        child_dir0 = sample_random(new_dir, 0.5)
        perp_child_dir0 = child_dir0 - new_dir * np.dot(child_dir0, new_dir)
        child_dir1 = child_dir0 - 2 * perp_child_dir0

        extend(new_base, child_dir0, segment_len * 0.5)
        extend(new_base, child_dir1, segment_len * 0.5)

    extend(origin, sample_random(up, 0.1), 1.0)
    return objects


def sample_random(normal, angle):
    """
    Randomly create an unit vector which is oriented
    by the given angle from normal.

    angle: radian
    """
    perp = np.random.randn(4)
    perp -= normal * np.dot(normal, perp)
    perp /= la.norm(perp)
    return math.cos(angle) * normal + math.sin(angle) * perp


def set_landscape(scene):
    """
    Set everything(trees, lands, lights) to scene.
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
