#!/bin/python2
from __future__ import print_function, division
"""
Run medium tests for pentatope, that mainly checks that
* all examples produce something without crashes
* pentatope responds correctly in server mode

protobuf is preventing migration to python3.
protobuf3 seems to support python3, but it's still alpha.
"""
from __future__ import print_function, division
from google.protobuf import text_format
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
import urllib2
sys.path.append('build/proto')
import render_server_pb2


class TestSceneExamples(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(dir="./")

        temp_dir_name = os.path.basename(self.temp_dir)
        assert(temp_dir_name != '')
        self.temp_dir_inside = os.path.join("/root/local", temp_dir_name)

        self.image_name = "xanxys/pentatope-dev"

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def run_pentatope(self, args):
        subprocess.call([
            "sudo", "docker", "run",
            "--tty=true",
            "--interactive=true",
            "--rm",
            "--volume", "/home/xyx/repos/pentatope:/root/local",
            self.image_name,
            "/root/local/build/pentatope"] + args)

    def test_anim_demo(self):
        subprocess.call([
            "example/animation_demo.py",
            "--test",
            "--output", os.path.join(self.temp_dir, "anim_demo.pb"),
            "--render_output", os.path.join(self.temp_dir_inside, 'anim_demo.png')
            ])
        self.run_pentatope([
            "--render", os.path.join(self.temp_dir_inside, "anim_demo.pb")])
        self.assertTrue(
            os.path.isfile(os.path.join(self.temp_dir, "anim_demo.png")))

    def test_cornell(self):
        # Reduce cornell.proto parameters to accelerate test.
        path_proto_temp = os.path.join(self.temp_dir, "cornell.prototxt")
        path_proto_temp_inside = os.path.join(self.temp_dir_inside, "cornell.prototxt")
        proto = open("example/cornell_tesseract.prototxt", "r").read() \
            .replace('output_path: "./render.png"',
                'output_path: "%s"' % os.path.join(self.temp_dir_inside, "cornell.png")) \
            .replace('sample_per_pixel: 100', 'sample_per_pixel: 1')
        with open(path_proto_temp, "w") as f:
            f.write(proto)
        # Run rendering.
        self.run_pentatope(["--render", path_proto_temp_inside])
        self.assertTrue(
            os.path.isfile(os.path.join(self.temp_dir, "cornell.png")))


class TestPentatopeServer(unittest.TestCase):
    def setUp(self):
        self.image_name = "xanxys/pentatope-dev"
        self.container_name = "pentatope_test_smoke"
        self.port = random.randint(35000, 50000)

        result = subprocess.check_output([
            "sudo", "docker", "run",
            "--detach=true",
            "--volume", "/home/xyx/repos/pentatope:/root/local",
            "--name", self.container_name,
            "--publish", "%d:80" % self.port,
            self.image_name,
            "/root/local/build/pentatope"],
            stderr=subprocess.STDOUT)
        self.container_id = result.decode('utf-8').strip()
        time.sleep(1)  # wait server boot

    def tearDown(self):
        subprocess.call([
            "sudo", "docker", "rm", "-f", self.container_id])

    def test_malformed_proto_raises_error(self):
        with self.assertRaises(urllib2.HTTPError) as cm:
            urllib2.urlopen(
                "http://localhost:%d" % self.port,
                "INVALID PROTO",
                5).read()
        self.assertEqual(cm.exception.code, 400)

    def test_responds_ok(self):
        # Create request
        render_request = render_server_pb2.RenderRequest()
        content = open('example/cornell_tesseract.prototxt').read()
        text_format.Merge(content, render_request.task)
        render_request.task.sample_per_pixel = 1
        render_request.task.camera.size_x = 64
        render_request.task.camera.size_y = 48
        # Fetch result.
        http_resp_body = urllib2.urlopen(
            "http://localhost:%d" % self.port,
            render_request.SerializeToString(),
            5).read()
        render_response = render_server_pb2.RenderResponse()
        render_response.ParseFromString(http_resp_body)
        # Verify result.
        self.assertTrue(render_response.is_ok)
        self.assertFalse(render_response.HasField("error_message"))
        self.assertGreater(len(render_response.output), 0)


if __name__ == '__main__':
    unittest.main()
