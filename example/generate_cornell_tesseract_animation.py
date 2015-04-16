#!/bin/python2
from __future__ import print_function, division
from google.protobuf import text_format
from render_task_pb2 import *
import argparse
import math
import numpy as np
import subprocess


def setup_cornell_animation(duration, fps):
    """
    duration: sec
    fps: frames per sec

    returns [(task path, image path)] sequentially
    """
    content = open('example/cornell_tesseract.prototxt').read()
    rt_base = RenderTask()
    text_format.Merge(content, rt_base)

    def populate_frame(camera_config, t):
        """
        Return RenderTask at time t (second).
        output_path will be undefined.
        """
        rt = RenderTask()
        rt.CopyFrom(rt_base)
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
        rot_per_sec_xy = 1 / 2
        rot_per_sec_xz = 1 / 3
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
        del rt.camera.local_to_world.rotation[:]
        rt.camera.local_to_world.rotation.extend(list(l_to_w_t.reshape([-1])))
        # set translation
        del rt.camera.local_to_world.translation[:]
        rt.camera.local_to_world.translation.extend(list(pos_t))
        return rt

    print('Render images directory: %s' % frames_path_prefix)
    print('Render frame description directory: %s' % tasks_path_prefix)

    n_frames = int(duration * fps)
    print('Needs %d (= %f s * %f f/s) frames' % (n_frames, duration, fps))

    task = proto.RenderMovieTask()
    task.framerate = fps
    task.sample_per_pixel = 100
    task.width, task.height = 640, 480
    task.scene.CopyFrom(rt_base.scene)  # TODO: need to represent as RenderScene
    for i in range(n_frames):
        frame = task.frames.add()
        populate_frame(frame, i / fps)


def process_task(task_path):
    print('Processing %s' % task_path)
    subprocess.check_call(['build/pentatope', '--render', task_path])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="""
Render animation that moves around center of cornell tesseract
at (X,Y): 1/2 rot/sec && (X,Z): 1/3 rot/sec""",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument(
        '--duration', type=float, default=5.0,
        help='Duration of animation')
    parser.add_argument(
        '--fps', type=float, default=30,
        help='frames / second')
    parser.add_argument(
        '--output', type=str, default=None, required=True,
        help='RenderMovieTask pb output path')

    args = parser.parse_args()

    animation = setup_cornell_animation(args.duration, args.fps)

    tasks, frames = zip(*animation)  # unzip
