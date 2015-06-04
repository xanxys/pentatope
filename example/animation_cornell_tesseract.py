#!/bin/python2
from __future__ import print_function, division
from google.protobuf import text_format
from render_task_pb2 import *
import argparse
import math
import numpy as np
import sys


def configure_camera(camera_config, t, image_size):
    """
    Set camera_config to time t (second).
    """
    camera_config.camera_type = "perspective2"
    camera_config.size_x, camera_config.size_y = image_size
    camera_config.fov_x = 157
    camera_config.fov_y = 150

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
    pos0 = np.array([0, -0.95, 0, 1])
    rot_per_sec_xy = 1 / 4
    rot_per_sec_xz = 1 / 5
    angle_xy = 2 * math.pi * rot_per_sec_xy * t
    angle_xz = 2 * math.pi * rot_per_sec_xz * t
    rot_xy = np.eye(4)
    rot_xy[0, 0] = math.cos(angle_xy)
    rot_xy[1, 0] = math.sin(angle_xy)
    rot_xy[0, 1] = -math.sin(angle_xy)
    rot_xy[1, 1] = math.cos(angle_xy)
    rot_xz = np.eye(4)
    rot_xz[0, 0] = math.cos(angle_xz)
    rot_xz[2, 0] = math.sin(angle_xz)
    rot_xz[0, 2] = -math.sin(angle_xz)
    rot_xz[2, 2] = math.cos(angle_xz)

    stage_to_world = np.dot(rot_xy, rot_xz)

    pos_t = np.dot(stage_to_world, pos0)
    l_to_w_t = np.dot(stage_to_world, local_to_stage)

    # set rotation
    del camera_config.local_to_world.rotation[:]
    camera_config.local_to_world.rotation.extend(list(l_to_w_t.reshape([-1])))
    # set translation
    del camera_config.local_to_world.translation[:]
    camera_config.local_to_world.translation.extend(list(pos_t))


def add_cornell_animation_frames(task, duration, fps, image_size):
    """
    duration: sec
    fps: frames per sec
    """
    n_frames = int(duration * fps)
    print('Generating %d (= %f s * %f f/s) frames' % (n_frames, duration, fps))

    task.framerate = fps
    task.sample_per_pixel = 100
    task.width, task.height = image_size
    for i in range(n_frames):
        frame = task.frames.add()
        configure_camera(frame, i / fps, image_size)


def load_cornell_scene(task):
    """
    task: Either RenderTask or RenderMovieTask.
    """
    content = open('example/cornell_tesseract_scene.prototxt').read()
    text_format.Merge(content, task.scene)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="""
Render animation that moves around center of cornell tesseract
at (X,Y): 1/2 rot/sec && (X,Z): 1/3 rot/sec""",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument(
        '--test', action='store_true',
        help='Low sample/px, framerate, resolution for quick testing.')
    # animation settings.
    parser.add_argument(
        '--duration', type=float, default=10.0,
        help='Duration of animation')
    # output options.
    parser.add_argument(
        '--output', type=str, default=None,
        help="Output path of RenderTask in .pb.")

    parser.add_argument(
        '--shot', type=str, default=None,
        help='Generate RenderMovieTask of a camera rotating in cornell tesseract.')

    # Parsing and validation.
    args = parser.parse_args()
    if args.output is None and args.shot is None:
        print("Either --output or --shot is required")
        sys.exit(1)

    # Create one kind of task.
    if args.test:
        fps = 5
        sample_per_pixel = 100
        image_size = (160, 120)
    else:
        fps = 30
        sample_per_pixel = 250
        image_size = (640, 480)

    if args.output is not None:
        task = RenderTask()
        load_cornell_scene(task)
        task.sample_per_pixel = sample_per_pixel

        configure_camera(task.camera, 0, image_size)
        with open(args.output, "wb") as f_task:
            f_task.write(task.SerializeToString())
    else:
        task = RenderMovieTask()
        load_cornell_scene(task)
        task.framerate = fps
        task.sample_per_pixel = sample_per_pixel
        task.width, task.height = image_size

        add_cornell_animation_frames(task, args.duration, fps, image_size)
        with open(args.shot, "wb") as f_movie_task:
            f_movie_task.write(task.SerializeToString())
