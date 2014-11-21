import os.path

env = Environment()

# manually emulate variant dir
proto_files = env.Command(
    ['build/proto/render_task.pb.cc', 'build/proto/render_task.pb.h'],
    ['proto/render_task.proto'],
    'protoc --proto_path=proto --cpp_out=build/proto $SOURCE')

env.Command(
    ['build/proto/render_task_pb2.py'],
    ['proto/render_task.proto'],
    'protoc --proto_path=proto --python_out=build/proto $SOURCE')

proto_files_cc = [f for f in proto_files if f.name.endswith('.pb.cc')]

SConscript(
    'src/SConscript',
    variant_dir='build',
    exports=['proto_files_cc'])
