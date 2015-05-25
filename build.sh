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

readonly SOURCE_TYPE=$1

# Export contents of the working directory to $output_path
pack_source_working() {
	local output_path=$1

	tar cvf $output_path \
		$(git ls-files --others --exclude-standard; git ls-files)
}

# Export spcified slice from git repository and put to output_path
pack_source_git() {
	local git_hash=$1
	local output_path=$2

	echo "Extracting $git_hash to $output_path"
	git archive -o $output_path $git_hash
}

# Export specified source code as tarball at output_path.
pack_source() {
	local output_path=$1

	case $SOURCE_TYPE in
		--working)
			echo "Using sources from current working directory"
			pack_source_working $output_path
			;;
		--head-local)
			echo "Using HEAD of local git"
			pack_source_git $(git log -n 1 --pretty=format:%H) \
				$output_path
			;;
		--head-origin)
			echo "Using HEAD of github"
			pack_source_git $(git log -n 1 --pretty=format:%H origin) \
				$output_path
			;;
		*)
			echo "Unknown soucre type option: $SOURCE_TYPE"
			exit 1
	esac
}

main() {
	echo "Creating base (1/4)"
	sudo docker build -t docker.io/xanxys/pentatope-base -f docker/base ./docker

	echo "Creating dev (2/4)"
	sudo docker build -t docker.io/xanxys/pentatope-dev -f docker/dev ./docker

	echo "Creating prod (3/4)"
	local pack_path=$(mktemp --suffix="-pentatope-src-pack.tar")
	local build_path=$(mktemp -d --suffix="-pentatope-build")
	pack_source $pack_path
	# Build inside dev container and extract all build product
	sudo rm -rf build-temp
	sudo docker run --rm \
		--volume $pack_path:/root/src-pack.tar \
		--volume $build_path:/root/build \
		xanxys/pentatope-dev \
		sh -c \
		'cd /root && \
		mkdir pentatope && \
		tar xvf src-pack.tar --directory pentatope && \
		cd pentatope && \
		scons -j 8 && \
		cp -Rv /root/pentatope/build/* /root/build'
	sudo chmod -R a+rw $build_path

	# Create prod image by copying only the binary
	cp $build_path/worker/worker docker/
	cp $build_path/controller/controller docker/
	sudo docker build -t docker.io/xanxys/pentatope-prod -f docker/prod ./docker

	# Copy python proto files to local directories (controller & example)
	cp $build_path/controller/controller ./pentatope_controller
	cp $build_path/proto/*.py example/
	cp $build_path/proto/*.py ./

	# Remove build artifacts immediately because they're not runnable outside
	# containers.
	sudo rm -rf $pack_path
	sudo rm -rf $build_path

	echo "Testing container (4/4)"
	# Run worker integration tests using local docker.
	./test_smoke.py

	echo "Done!"
}

main
