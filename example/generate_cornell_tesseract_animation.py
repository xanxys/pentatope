#!/bin/python2
from __future__ import print_function, division
import sys
sys.path.append('build/proto')
from render_task_pb2 import *
from google.protobuf import text_format
import numpy as np
import math
import os.path
import tempfile
import subprocess

if __name__ == '__main__':
    content = open('example/cornell_tesseract.prototxt').read()
    rt_base = RenderTask()
    text_format.Merge(content, rt_base)

    tasks_path_prefix = tempfile.mkdtemp()
    frames_path_prefix = tempfile.mkdtemp()

    def create_frame(t):
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
        # by (X,Y)-rotation
        pos0 = np.array([0, -0.95, 0, 1])
        rot_per_sec = 1.0
        angle = 2 * math.pi * rot_per_sec * t
        stage_to_world = np.eye(4)
        stage_to_world[0, 0] = math.cos(angle)
        stage_to_world[1, 0] = math.sin(angle)
        stage_to_world[0, 1] = -math.sin(angle)
        stage_to_world[1, 1] = math.cos(angle)

        pos_t = np.dot(stage_to_world, pos0)
        l_to_w_t = np.dot(stage_to_world, local_to_stage)

        # set rotation
        del rt.camera.local_to_world.rotation[:]
        rt.camera.local_to_world.rotation.extend(list(l_to_w_t.reshape([-1])))
        # set translation
        del rt.camera.local_to_world.translation[:]
        rt.camera.local_to_world.translation.extend(list(pos_t))
        return rt

    def create_frame_task(t, index):
        """
        Save RenderTask for the frame at (time t, index) on disk
        as binary proto.
        Returns (full path of RenderTask proto, full path of frame image)
        """
        rt = create_frame(t)
        task_path = os.path.join(tasks_path_prefix, '%d.pb' % index)
        frame_path = os.path.join(frames_path_prefix, '%d.png' % index)
        rt.output_path = frame_path
        open(task_path, 'wb').write(rt.SerializeToString())
        return (task_path, frame_path)

    print('Render images directory: %s' % frames_path_prefix)
    print('Render frame description directory: %s' % tasks_path_prefix)

    duration = 5
    fps = 30

    n_frames = int(duration * fps)
    print('Needs %d (= %f s * %f f/s) frames' % (n_frames, duration, fps))

    tasks = []
    frames = []
    for i in range(n_frames):
        task, frame = create_frame_task(i / fps, i)
        tasks.append(task)
        frames.append(frame)

    # Run tasks
    for (i, task) in enumerate(tasks):
        print('Rendering frame %d of %d' % (i + 1, n_frames))
        subprocess.check_call(['build/pentatope', '--render', task])

