#!/bin/python2
from __future__ import print_function, division
import argparse
import sys
import numpy as np
import math
import scene_nature
sys.path.append('build/proto')
import render_task_pb2 as proto


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="""
Generate a scene containing a fractal landscape and trees.""",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument(
        '--test', action='store_true',
        help='Lower sample/px and smaller resolution for quick testing.')
    args = parser.parse_args()

    task = proto.RenderTask()
    if args.test:
        task.sample_per_pixel = 100
    else:
        task.sample_per_pixel = 250
    task.output_path = "./demo-frame.png"

    t = 0
    rot_per_sec_xy = 1
    # rotation:
    # stage <- Camera
    # X+         X+(horz)
    # W+         Y-(up)
    # Z+         Z+(ignored)
    # Y+         W+(forward)
    local_to_stage = np.array([
        [1, 0, 0, 0],
        [0, 0, 0, 1],
        [0, 0, 1, 0],
        [0, -1, 0, 0]])
    # world <- stage
    # lookat (0, 0, 0, 1)
    # by (X,Y) & (X,Z)-rotation
    pos0 = np.array([0, 0, 0, 3])
    # rot_per_sec_xy = 1 / 2
    # rot_per_sec_xz = 1 / 3
    # angle_xy = 2 * math.pi * rot_per_sec_xy * t
    # angle_xz = 2 * math.pi * rot_per_sec_xz * t
    # rot_xy = np.eye(4)
    # rot_xy[0, 0] = math.cos(angle_xy)
    # rot_xy[1, 0] = math.sin(angle_xy)
    # rot_xy[0, 1] = -math.sin(angle_xy)
    # rot_xy[1, 1] = math.cos(angle_xy)
    # rot_xz = np.eye(4)
    # rot_xz[0, 0] = math.cos(angle_xz)
    # rot_xz[2, 0] = math.sin(angle_xz)
    # rot_xz[0, 2] = -math.sin(angle_xz)
    # rot_xz[2, 2] = math.cos(angle_xz)

    # stage_to_world = np.dot(rot_xy, rot_xz)
    stage_to_world = np.eye(4)

    pos_t = np.dot(stage_to_world, pos0)
    l_to_w_t = np.dot(stage_to_world, local_to_stage)

    task.camera.camera_type = "perspective2"
    if args.test:
        task.camera.size_x = 160
        task.camera.size_y = 120
    else:
        task.camera.size_x = 640
        task.camera.size_y = 480
    task.camera.fov_x = 157
    task.camera.fov_y = 150
    task.camera.local_to_world.rotation.extend(list(l_to_w_t.flatten()))
    task.camera.local_to_world.translation.extend(list(pos_t))

    scene = task.scene
    scene_nature.add_landscape(scene)

    with open("task.pb", "wb") as f_task:
        f_task.write(task.SerializeToString())
