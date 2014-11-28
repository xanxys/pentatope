FROM fedora:20

# Basic stuff
RUN yum update -y; \
    yum install -y scons git clang nano libstdc++ libstdc++-devel gcc-c++; \
    yum clean all

# Project-dependent stuff
RUN yum install -y \
    boost boost-devel \
    protobuf protobuf-compiler gtest gtest-devel glog-devel protobuf-devel \
    eigen3-devel opencv-devel \
    numpy scipy pycairo; \
    yum clean all
# pcl: 1.7.1
# eigen: 3.2.2
# opencv: 2.4.7
# boost: 1.53.0

# Get Project and compile
RUN cd /root; \
    git clone --depth=1 https://github.com/xanxys/pentatope; \
    cd pentatope; \
    scons -j 8
