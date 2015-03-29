#!/bin/python2
from __future__ import print_function, division
import argparse
import boto
import boto.ec2
import json
import math
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
import tornado.httpclient
import urllib2
sys.path.append('build/proto')
from render_task_pb2 import *
from render_server_pb2 import *


class IncompatibleAPIError(Exception):
    """
    Raised when (often unofficial) API changes.
    """
    pass


class InvalidTaskError(Exception):
    """
    Raised when user-specified task is un-executable.
    """
    pass


class AWSPrice(object):
    """
    An abstract object that holds current AWS prices.
    """
    def __init__(self):
        self.ec2_spot = self._get_ec2_spot()

    def _get_ec2_spot(self):
        result = urllib2.urlopen("https://spot-price.s3.amazonaws.com/spot.js").read()
        # Strip non-JSON things.
        prefix = "callback("
        postfix = ")"
        if not result.startswith(prefix) or not result.endswith(postfix):
            raise IncompatibleAPIError()
        result = result[len(prefix):-len(postfix)]
        # 
        try:
            return json.loads(result)
        except ValueError as exc:
            raise IncompatibleAPIError("Expecting json %s" % exc)

    def get_ec2_spot(self, query_instance):
        """
        Return [(region name, price in USD)]
        """
        results = []
        try:
            for region in self.ec2_spot["config"]["regions"]:
                region
                for types in region["instanceTypes"]:
                    if types["type"] != "computeCurrentGen":
                        continue
                    for ty in types["sizes"]:
                        if ty["size"] != query_instance:
                            continue
                        for vals in ty["valueColumns"]:
                            if vals["name"] != "linux":
                                continue
                            results.append((
                                region["region"], float(vals["prices"]["USD"])))
        except KeyError:
            raise IncompatibleAPIError("AWS price table format has changed")
        if len(results) == 0:
            raise IncompatibleAPIError(
                "AWS price table format has changed, or specified instance is no longer available.")
        return results


def plan_action(n_samples):
    """
    Current computation is number of samples
    (= #frames * pixel/frame * sample/pixel)
    """
    # TODO: measure it accurately
    inst_type = "c3.8xlarge"
    samples_per_hour = 32 * 3600 * 1000 * 1000

    aws_price = AWSPrice()
    spot = aws_price.get_ec2_spot(inst_type)

    # Amazon Linux (HVM, instance-store), 2014.09.1
    # https://aws.amazon.com/jp/amazon-linux-ami/
    amis = {
        "us-east": "ami-0268d56a",
        "apac-tokyo": "ami-8985b088"
    }

    spot = [(reg, pr) for (reg, pr) in spot if reg in amis]
    best_spot = min(spot, key=lambda (region, price): price)
    n_instances = int(math.ceil(n_samples / samples_per_hour))

    return {
        "ec2": {
            "type": inst_type + " (spot)",
            "region": best_spot[0],
            "ami": amis[best_spot[0]],
            "n_instances": n_instances
        },
        "price": best_spot[1] * n_instances,
        "time": n_samples / (n_instances * samples_per_hour)
    }


def run_task():
    print("Rendering frames")
    print("{0: <64}|{1: >15}".format(
        "Worker Status (##terminated  ==rendering ..requested)",
        "Rendered Frames"))

    total_frames = 10

    def refresh_status(
            p_terminated, p_booted, p_requested,
            finished_frames):
        cols = 65
        n = cols - 2  # exclude "[" and "]"
        segs = ["#", "=", ".", " "]
        widths = [
            int(p_terminated * n),
            int((p_booted - p_terminated) * n),
            int((p_requested - p_booted) * n)
        ]
        widths.append(n - sum(widths))

        assert(len(segs) == len(widths))
        bar_s = "["
        for (seg, width) in zip(segs, widths):
            bar_s += seg * width
        bar_s += "]"

        # 7
        frames_s = "%8d (%3d%%)" % (finished_frames,
            int(100 * finished_frames / total_frames))

        print("{0: <65}{1: >15}".format(bar_s, frames_s),
            end='\r')
        sys.stdout.flush()

    stat = {}
    stat["finished_frames"] = 0
    refresh_status(0.05, 0.8, 0.99, stat["finished_frames"])

    def handle_request(response):
        stat["finished_frames"] += 1
        refresh_status(0.05, 0.8, 0.99, stat["finished_frames"])

        if stat["finished_frames"] == total_frames:
            print("")
            tornado.ioloop.IOLoop.instance().stop()


    http_client = tornado.httpclient.AsyncHTTPClient()
    for i in range(total_frames):
        http_client.fetch("http://www.xanxys.net/index.html", callback=handle_request)
    tornado.ioloop.IOLoop.instance().start()

    print("Encoding the results")

    print("Done! Results written to hogehoge")


class LocalProvider(object):
    """
    Provides local pentatope instances.
    LocalProvier assumes docker to be installed on the local system,
    and sudo works without password.
    """
    def calc_bill(self):
        return [
            ("This machine", 0)]

    def prepare(self):
        self.image_name = "xanxys/pentatope-dev"
        self.container_name = "pentatope_local_worker_%d" % random.randint(0, 1000)
        self.port = random.randint(35000, 50000)

        result = subprocess.check_output([
            "sudo", "docker", "run",
            "--detach=true",
            "--volume", "/home/xyx/repos/pentatope:/root/local",
            "--name", self.container_name,
            "--publish", "%d:80" % self.port,
            self.image_name,
            "/root/local/build/pentatope"])
        self.container_id = result.decode('utf-8').strip()
        time.sleep(1)  # wait server boot
        return ["http://localhost:%d/" % self.port]

    def discard(self):
        subprocess.call([
            "sudo", "docker", "rm", "-f", self.container_id])


class EC2Provider(object):
    """
    Provides remote pentatope instances on AWS EC2.
    """
    def __init__(self, credentials):
        pass

    def calc_bill(self):
        pass
    
    def prepare(self):
        pass

    def discard(self):
        pass


def render_movie(providers, in_path, out_path):
    """
    Execute a RenderMovieTask (loaded from in_oath) and
    output H264 movie to out_path.
    This function use providers for actual calculation.
    """
    task = RenderMovieTask()
    with open(in_path, "rb") as f_task:
        task.ParseFromString(f_task.read())

    if len(task.frames) == 0:
        raise InvalidTaskError("One or more frames required")

    instances = []
    for provider in providers:
        instances += provider.prepare()
    assert(len(instances) > 0)

    images_path_prefix = tempfile.mkdtemp()

    try:
        # Render frames and collect image paths
        image_paths = []
        for (i_frame, frame_camera) in enumerate(task.frames):
            path_image = os.path.join(
                images_path_prefix, "frame-%06d.png" % i_frame)

            task_frame = RenderTask()
            task_frame.sample_per_pixel = task.sample_per_pixel
            task_frame.camera.CopyFrom(frame_camera)
            task_frame.scene.CopyFrom(task.scene)

            render_request = RenderRequest()
            render_request.task.CopyFrom(task_frame)

            print('Rendering frame %d of %d' % (i_frame + 1, len(task.frames)))
            response = urllib2.urlopen(
                instances[0],
                render_request.SerializeToString(),
                300).read()
            render_response = RenderResponse()
            render_response.ParseFromString(response)
            if not render_response.is_ok:
                print("Server error: %s" % render_response.error_message)

            with open(path_image, "wb") as f_image:
                f_image.write(render_response.output)
            image_paths.append(path_image)
        assert(len(image_paths) == len(task.frames))

        # Encode frame images to a single h264 movie.
        command = [
            "ffmpeg",
            "-y",  # overwrite
            "-framerate", str(task.framerate),
            "-i", os.path.join(images_path_prefix, "frame-%06d.png"),
            "-pix_fmt", "yuv444p",
            "-crf", "18",  # visually lossless
            "-c:v", "libx264",
            "-loglevel", "warning",
            "-r", str(task.framerate),
            out_path]
        print('Encoding with command: %s' % command)
        subprocess.check_call(command)
    finally:
        shutil.rmtree(images_path_prefix)
        for provider in providers:
            provider.discard()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="""Render given animation.""",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    # Computation platform.
    parser.add_argument(
        '--local',
        help="Use this machine.",
        action='store_true')
    parser.add_argument(
        '--aws',
        help="Use Amazon Web Services with a credential file.",
        type=str, default=None)
    # Inputs.
    parser.add_argument(
        '--input',
        help="Input .pb file containing an animation.",
        type=str, required=True)
    # Outputs.
    parser.add_argument(
        '--output-mp4',
        help="Encode the results as H264/mp4.",
        type=str, default=None)

    # Validate.
    args = parser.parse_args()
    if not args.local and args.aws is None:
        print("You must specify at least one computation platform.", file=sys.stderr)
        sys.exit(1)

    if args.aws is not None:
        aws_cred = json.load(open(args.aws))

    if args.output_mp4 is None:
        print("You should specify one or more output format.", file=sys.stderr)
        sys.exit(1)

    # Create resource providers and show cost estimate.
    providers = []
    if args.local:
        providers.append(LocalProvider())
    if args.aws is not None:
        raise NotImplementedError("--aws is not implemented yet")

    print('{:=^80}'.format(" Estimated Price "))
    total_price = 0
    for provider in providers:
        for bill in provider.calc_bill():
            total_price += bill[1]
            print("{0:<65}|{1:>10.2f} USD".format(*bill))
    print('-' * 80)
    print("{:>76.2f} USD".format(total_price))

    # Ask to proceed.
    print("Are you sure? [y/N]", end='')
    sys.stdout.flush()
    if sys.stdin.readline().strip() != 'y':
        print("Aborted.")
        sys.exit(0)

    render_movie(providers, args.input, args.output_mp4)
