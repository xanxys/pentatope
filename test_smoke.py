#!/bin/python3
import os
import shutil
import subprocess
import tempfile
import unittest


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
            .replace('sample_per_pixel: 100', 'sample_per_pixel: 3')
        open(path_proto_temp, "w").write(proto)
        # Run rendering.
        self.run_pentatope(["--render", path_proto_temp_inside])
        self.assertTrue(
            os.path.isfile(os.path.join(self.temp_dir, "cornell.png")))


if __name__ == '__main__':
    unittest.main()
