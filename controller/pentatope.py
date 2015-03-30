#!/bin/python2
from __future__ import print_function, division
from provider import LocalProvider, EC2Provider, ProviderValidationError
from render_server_pb2 import *
from render_task_pb2 import *
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import tornado.httpclient
import traceback
import urllib2


class InvalidTaskError(Exception):
    """
    Raised when user-specified task is un-executable.
    """
    pass


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
            # Because Python2 is broken (i.e. no bytes type),
            # we cannot pass unicode URL. Otherwise, it will try to parse
            # data as utf-8 and fail.
            response = urllib2.urlopen(
                instances[0].encode('ascii'),
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
    except BaseException:
        print("Error happened during rendering. exiting.")
        traceback.print_exc()
    finally:
        shutil.rmtree(images_path_prefix)
        for provider in providers:
            provider.discard()


def is_ffmpeg_installed():
    try:
        subprocess.check_output(["ffmpeg", "-version"])
        return True
    except OSError:
        return False


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
    if args.output_mp4:
        if not is_ffmpeg_installed():
            print("ffmpeg need to be installed to use mp4 output")
            sys.exit(1)

    if args.output_mp4 is None:
        print("You should specify one or more output format.", file=sys.stderr)
        sys.exit(1)

    # Create resource providers and show cost estimate.
    providers = []
    if args.local:
        try:
            providers.append(LocalProvider())
        except ProviderValidationError as exc:
            print("Canot use local machine because %s" % exc)

    if args.aws is not None:
        try:
            aws_credential = json.load(open(args.aws))
            try:
                providers.append(EC2Provider(aws_credential))
            except ProviderValidationError as exc:
                print("Cannot use AWS because %s" % exc)
        except:
            print("Failed to open AWS credential, ignoring --aws.")

    if not providers:
        print(
            "You must specify at least one usable computation provider.",
            file=sys.stderr)
        sys.exit(1)

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
