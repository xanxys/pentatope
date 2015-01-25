#!/bin/python2
from __future__ import print_function, division
import sys
sys.path.append('build/proto')
from render_task_pb2 import *
from render_server_pb2 import *
import argparse
import urllib2
import json
import math
import curses
import tornado.httpclient
import time

class IncompatibleAPIError(Exception):
    """
    Raised when (often unofficial) API changes.
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
    best_spot = min(spot, key=lambda (region, price): price)
    n_instances = int(math.ceil(n_samples / samples_per_hour))

    return {
        "ec2": {
            "type": inst_type + " (spot)",
            "region": best_spot[0],
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

    args = parser.parse_args()

    if not args.local and args.aws is None:
        print("You must specify at least one computation platform.", file=sys.stderr)
        sys.exit(1)
    
    if args.output_mp4 is None:
        print("You should specify one or more output format.", file=sys.stderr)
        sys.exit(1)

    # 1 minute clip @ 60fps, 1080p, 100sample/px
#    n_samples = 60 * 60 * 1920 * 1080 * 100
#    est = plan_action(n_samples)

#    print("== Estimated Time & Price ==")
#    print("* time: %.1f hour" % est["time"])
#    print("* price: %.1f USD" % est["price"])
#    print("== Price Breakup ==")
#    print("* AWS EC2: %s, %s, spot %d nodes" %
#        (est["ec2"]["type"], est["ec2"]["region"], est["ec2"]["n_instances"]))

#    while True:
#        result = raw_input("Do you want to proceed? (y/n)")
#        if result == 'y':
#            break
#        elif result == 'n':
#            sys.exit(0)

    print("Running tasks")
    run_task()
