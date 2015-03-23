FROM fedora:21

# Confirmed versions
# pcl: 1.7.1
# eigen: 3.2.2
# opencv: 2.4.7
# boost: 1.53.0

# Runtime
RUN yum update -y; \
    yum install -y \
    boost \
    protobuf gflags glog \
    eigen3 opencv; \
    yum clean all

# Dev tools
RUN yum install -y \
    boost-devel \
    protobuf-devel protobuf-compiler gflags-devel gtest-devel glog-devel \
    eigen3-devel opencv-devel \
    numpy scipy pycairo \
    nano scons git clang gcc-c++ libstdc++-devel; \
    yum clean all

# Get Project and compile
RUN cd /root; \
    git clone --depth=1 https://github.com/xanxys/pentatope; \
    cd pentatope; \
    scons -j 8
