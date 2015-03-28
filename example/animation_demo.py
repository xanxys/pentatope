#!/bin/python2
from __future__ import print_function, division
import argparse
import sys
import numpy as np
import math
import scene_nature
sys.path.append('build/proto')
import render_task_pb2 as proto


def camera_config_nature_duration():
    return 1.0

def camera_config_nature(t, args, camera_config, image_size):
    """
    Set CameraConfig at time t in nature scene.
    """
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
    pos0 = np.array([0, 0, t * 2, 4])

    # stage_to_world = np.dot(rot_xy, rot_xz)
    stage_to_world = np.eye(4)

    pos_t = np.dot(stage_to_world, pos0)
    l_to_w_t = np.dot(stage_to_world, local_to_stage)

    camera_config.camera_type = "perspective2"
    camera_config.size_x, camera_config.size_y = image_size
    camera_config.fov_x = 157
    camera_config.fov_y = 150
    camera_config.local_to_world.rotation.extend(list(l_to_w_t.flatten()))
    camera_config.local_to_world.translation.extend(list(pos_t))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="""
Generate one of the shots of scenes.""",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    # common flag
    parser.add_argument(
        '--test', action='store_true',
        help='Lower sample/px and smaller resolution for quick testing.')
    # command flag
    parser.add_argument(
        '--output', type=str,
        help="Output path of RenderTask in .pb.")
    parser.add_argument(
        '--render_output', type=str,
        help="Deferred output path of rendered image.")

    parser.add_argument(
        '--shot_nature', type=str,
        help='Generate RenderMovieTask of a shot of a nature scene.')
    args = parser.parse_args()

    # verify command
    if args.output is not None:
        if args.render_output is None:
            print("--render_output is required for --output")
            sys.exit(1)
    elif args.shot_nature is not None:
        pass
    else:
        print("Either --output or --shot_nature is required")
        sys.exit(1)

    if args.test:
        sample_per_pixel = 15
        image_size = (160, 120)
    else:
        sample_per_pixel = 250
        image_size = (640, 480)
    fps = 30

    if args.output is not None:
        task = proto.RenderTask()
        task.sample_per_pixel = sample_per_pixel
        task.output_path = args.render_output
        camera_config_nature(0, args, task.camera, image_size)
        scene_nature.set_landscape(task.scene)
        with open(args.output, "wb") as f_task:
            f_task.write(task.SerializeToString())
    elif args.shot_nature is not None:
        task = proto.RenderMovieTask()
        task.framerate = fps
        task.sample_per_pixel = sample_per_pixel
        task.width, task.height = image_size

        n_frames = int(camera_config_nature_duration() * fps)
        print("Generating %d frames @ %f fps" % (n_frames, fps))
        for i in range(n_frames):
            frame_cam = task.frames.add()
            camera_config_nature(i / fps, args, frame_cam, image_size)

        scene_nature.set_landscape(task.scene)
        with open(args.shot_nature, "wb") as f_task:
            f_task.write(task.SerializeToString())
