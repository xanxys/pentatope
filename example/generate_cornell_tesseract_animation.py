#!/bin/python2
from __future__ import print_function, division
import sys
sys.path.append('../build/proto')
from render_task_pb2 import *
from google.protobuf import text_format

if __name__ == '__main__':
    content = open('./cornell_tesseract.prototxt').read()
    rt_base = RenderTask()
    text_format.Merge(content, rt_base)

    def create_frame(t):
        """
        Return RenderTask at time t (second).
        """
        rt = RenderTask()
        rt.CopyFrom(rt_base)
        return rt

    fps = 30
    frames = []
    for i in range(100):
        frames.append(create_frame(i / fps))
    print(frames)
