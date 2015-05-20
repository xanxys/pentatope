#!/bin/bash
# This code creates 3 container images:
# 1 base: provides runtime libraries
# 2. dev (built on base), used for compilation and debugging
# 3. prod (built on base), used for running nodes on EC2 or local machine.
# 
# then, it uploads prod image as xanxys/dev to dockerhub for EC2 deployment.
#
# prod is used both for running C++ worker code in EC2 and running controller
# locally, but one image does both because binaries are pretty small and
# we don't want to increase complexity for tiny reduction in image size.
#
set -e  # exit immediately after an error

echo "Creating base (1/4)"
sudo docker build -t xanxys/pentatope-base -f docker/base ./docker

echo "Creating dev (2/4)"
sudo docker build -t xanxys/pentatope-dev -f docker/dev ./docker

echo "Creating prod (3/4)"
# Build inside dev container and extract all build product
sudo rm -rf build-temp
sudo docker run --rm \
	--volume $(pwd):/root/local \
	xanxys/pentatope-dev \
	sh -c \
	'cd /root && \
	git clone --depth=1 https://github.com/xanxys/pentatope && \
	cd pentatope && \
	scons -j 8 && \
	cp -Rv /root/pentatope/build /root/local/build-temp/'
sudo chmod -R a+rw build-temp

# Create prod image by copying only the binary
cp build-temp/worker/worker docker/
cp build-temp/controller/controller docker/
sudo docker build -t xanxys/pentatope-prod -f docker/prod ./docker

# Copy python proto files to local directories (controller & example)
cp build-temp/controller/controller ./pentatope_controller
cp build-temp/proto/*.py example/

# Remove build artifacts immediately because they're not runnable outside
# containers.
sudo rm -rf build-temp

echo "Pushing prod (4/4) (This step could fail you're not xanxys)"
# Run worker integration tests using local docker.
./test_smoke.py

sudo docker push xanxys/pentatope-prod

echo "Done!"
