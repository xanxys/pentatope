FROM ubuntu:14.10

# Runtime
RUN apt-get update; \
    apt-get install -y \
    libstdc++6 \
    libboost-filesystem1.55.0 \
    libboost-program-options1.55.0 \
    libboost-system1.55.0 \
    libboost-thread1.55.0 \
    libprotobuf8 \
    libgflags2 libgoogle-glog0 \
    libopencv2.4
    libcppnetlib0; \
    apt-get clean

# Dev tools
RUN apt-get install -y \
    libstdc++-4.8-dev libboost-dev \
    libboost-filesystem1.55-dev \
    libboost-program-options1.55-dev \
    libboost-system1.55-dev \
    libboost-thread1.55-dev \
    libprotobuf-dev protobuf-compiler \
    libgflags-dev libgtest-dev libgoogle-glog-dev \
    libeigen3-dev \
    libopencv-dev \
    libcppnetlib-dev \
    python-numpy python-scipy \
    nano scons cmake make git clang valgrind; \
    apt-get clean; \
    # gtest library build
    cp -R /usr/src/gtest /root/gtest; \
    cd /root/gtest; \
    cmake .; \
    make -j 4; \
    cp libgtest.a libgtest_main.a /usr/lib/x86_64-linux-gnu/

# Get Project and compile
RUN cd /root; \
    git clone --depth=1 https://github.com/xanxys/pentatope; \
    cd pentatope; \
    scons -j 8
