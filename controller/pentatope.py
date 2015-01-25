#!/bin/python2
from __future__ import print_function, division
import sys
sys.path.append('build/proto')
from render_task_pb2 import *
from render_server_pb2 import *
import argparse

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

    print("== Estimated Time & Price ==")
    print("* time: 30 minutes")
    print("* price: 5.5 USD")
    print("== Price Breakup ==")
    print("* AWS EC2: c3.xlarge, ap-north-east, spot     15 nodes: 3.5 USD")

    while True:
        result = raw_input("Do you want to proceed? (y/n)")
        if result == 'y':
            break
        elif result == 'n':
            sys.exit(0)

    print("Running tasks")