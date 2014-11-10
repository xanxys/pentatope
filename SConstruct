# default header
import os

if os.path.exists('/usr/bin/clang++'):
    compiler = 'clang++'
else:
    compiler = 'g++'

env = Environment(
    CXX=compiler,
    CXXFLAGS='-std=c++11 -Wall -Wextra',
    CCFLAGS=['-O3', '-g'],
    CPPPATH=[
        '/usr/include/eigen3',
        './',
    ],
    LIBPATH=[
    ],
    CPPDEFINES=[
        # 'ENABLE_USB_IO'
    ]
    )
env['ENV']['TERM'] = os.environ.get('TERM', '')

# project specific code
LIBS = [
    'libboost_filesystem',
    'libboost_program_options',
    'libboost_system',
    'libboost_thread',
    'libdl',
    'libglog',
    'libopencv_core',
    'libopencv_features2d',
    'libopencv_highgui',
    'libopencv_imgproc',
    'libpthread',
]

env.Program(
    'pentatope',
    source=[
        env.Object(f) for f
        in env.Glob('*.cpp') + env.Glob('*/*.cpp') + env.Glob("*/*.c") + env.Glob('*.cc')
        if not f.name.endswith('_test.cpp')],
    LIBS=LIBS)

program_test = env.Program(
    'pentatope_test',
    source=[
        env.Object(f) for f
        in env.Glob('*.cpp') + env.Glob('*/*.cpp') + env.Glob("*/*.c") + env.Glob('*.cc')
        if f.name != 'main.cpp'],
    LIBS=LIBS + ['libgtest', 'libgtest_main'])

env.Command('test', None, './' + program_test[0].path)
env.Depends('test', 'pentatope_test')
