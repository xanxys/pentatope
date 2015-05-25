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
import render_server_pb2


class TestSceneExamples(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(dir="./")

        temp_dir_name = os.path.basename(self.temp_dir)
        assert(temp_dir_name != '')
        self.temp_dir_inside = os.path.join("/root/local", temp_dir_name)

        self.image_name = "docker.io/xanxys/pentatope-prod"

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def run_pentatope(self, args):
        subprocess.call([
            "sudo", "docker", "run",
            "--tty=true",
            "--interactive=true",
            "--rm",
            "--volume", "%s:/root/local" % os.getcwd(),
            self.image_name,
            "/root/pentatope/worker"] + args)

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

    def test_static_cornell(self):
        subprocess.call([
            "example/animation_cornell_tesseract.py",
            "--test",
            "--output", os.path.join(self.temp_dir, "stat_cornell.pb"),
            "--render_output", os.path.join(self.temp_dir_inside, "stat_cornell.png"),
            ])
        self.run_pentatope([
            "--render", os.path.join(self.temp_dir_inside, "stat_cornell.pb")])
        self.assertTrue(
            os.path.isfile(os.path.join(self.temp_dir, "stat_cornell.png")))


class TestPentatopeServer(unittest.TestCase):
    def setUp(self):
        self.image_name = "docker.io/xanxys/pentatope-prod"
        self.container_name = "pentatope_test_smoke"
        self.port = random.randint(35000, 50000)

        result = subprocess.check_output([
            "sudo", "docker", "run",
            "--detach=true",
            "--name", self.container_name,
            "--publish", "%d:80" % self.port,
            self.image_name,
            "/root/pentatope/worker"],
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
        content = open('example/cornell_tesseract_scene.prototxt').read()
        text_format.Merge(content, render_request.task.scene)
        render_request.task.sample_per_pixel = 1
        render_request.task.camera.size_x = 64
        render_request.task.camera.size_y = 48
        render_request.task.camera.fov_x = 157
        render_request.task.camera.fov_y = 150
        render_request.task.camera.camera_type = "perspective2"
        render_request.task.camera.local_to_world.rotation.extend([
            1, 0, 0, 0,
            0, 0, 0, 1,
            0, 0, 1, 0,
            0, -1, 0, 0])
        render_request.task.camera.local_to_world.translation.extend([
            0, -0.95, 0, 1])
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
