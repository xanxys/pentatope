import os.path

env = Environment()

# manually emulate variant dir
proto_files = []

for name in ["physics_base", "render_server", "render_task", "scene"]:
    proto_files += env.Command(
        ['build/proto/%s.pb.cc' % name, 'build/proto/%s.pb.h' % name],
        ['proto/%s.proto' % name],
        'protoc --proto_path=proto --cpp_out=build/proto $SOURCE')

    proto_files += env.Command(
        ['build/proto/%s_pb2.py' % name],
        ['proto/%s.proto' % name],
        'protoc --proto_path=proto --python_out=build/proto $SOURCE')

proto_files += env.Command(
    ['build/controller/pentatope/%s.pb.go' % name],
    Glob("proto/*.proto"),
    'protoc --proto_path=proto --gogo_out=build/controller/pentatope proto/*')

proto_files_cc = [f for f in proto_files if f.name.endswith('.pb.cc')]
proto_files_go = [f for f in proto_files if f.name.endswith('.pb.go')]

SConscript(
    'worker/SConscript',
    variant_dir='build/worker',
    exports=['proto_files_cc'])
SConscript(
    'controller/SConscript',
    variant_dir='build/controller',
    exports=['proto_files_go'])
