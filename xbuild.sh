#!/bin/bash

# fpassman: cross-build on Linux for Linux/AMD64 | Windows/AMD64

IMAGE_NAME=fpassman-debianbw-builder
CONTAINER_NAME=fpassman_debianBW_build
BUILD_TARGET=linux
if test "$OS" == "windows" ; then
	IMAGE_NAME=fpassman-win64-builder
	CONTAINER_NAME=fpassman_win64_build
	BUILD_TARGET=mingw64
fi
ARGS=${@@Q}

set -xe

if ! test -d "../fpassman" ; then
	exit 1
fi

if ! podman container exists $CONTAINER_NAME ; then
	if ! podman image exists $IMAGE_NAME ; then

		# Create builder image
		if test "$OS" == "windows" ; then

			cat <<EOF | podman build -t $IMAGE_NAME -f - .
FROM debian:bookworm-slim
RUN apt update && \
 apt install -y \
  make
RUN apt install -y \
 zstd zip unzip bzip2 xz-utils \
 perl \
 cmake \
 patch \
 dos2unix \
 curl
RUN apt install -y \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
RUN apt install -y \
 gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
EOF

		else

			cat <<EOF | podman build -t $IMAGE_NAME -f - .
FROM debian:bookworm-slim
RUN apt update && \
 apt install -y \
  make
RUN apt install -y \
 zstd zip unzip bzip2 xz-utils \
 perl \
 cmake \
 patch \
 dos2unix \
 curl
RUN apt install -y \
 autoconf libtool libtool-bin \
 gettext \
 pkg-config
RUN apt install -y \
 gcc g++
RUN apt install -y \
 libgtk-3-dev
EOF
		fi
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 --name $CONTAINER_NAME \
	 $IMAGE_NAME \
	 bash -c "cd /src/fpassman && source ./build_$BUILD_TARGET.sh"
fi

if ! podman container top $CONTAINER_NAME ; then
	cat >build_$BUILD_TARGET.sh <<EOF
sleep 600
EOF
	# Start container in background
	podman start --attach $CONTAINER_NAME &
	sleep .5
	while ! podman container top $CONTAINER_NAME ; do
		sleep .5
	done
fi

# Prepare build script
if test "$OS" == "windows" ; then

	cat >build_mingw64.sh <<EOF
set -xe

mkdir -p ../ffpack/_windows-amd64
make -j8 zlib \
 -C ../ffpack/_windows-amd64 \
 -f ../Makefile \
 -I .. \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32-

mkdir -p cryptolib3/_windows-amd64
make -j8 \
 -C cryptolib3/_windows-amd64 \
 -f ../Makefile \
 -I .. \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32-

mkdir -p _windows-amd64
make -j8 \
 -C _windows-amd64 \
 -f ../Makefile \
 ROOT=../.. \
 OS=windows \
 COMPILER=gcc \
 CROSS_PREFIX=x86_64-w64-mingw32- \
 CFLAGS_USER=-fno-diagnostics-color \
 $ARGS
EOF

else

	cat >build_$BUILD_TARGET.sh <<EOF
set -xe

mkdir -p ../ffpack/_linux-amd64
make -j8 zlib \
 -C ../ffpack/_linux-amd64 \
 -f ../Makefile \
 -I ..

mkdir -p cryptolib3/_linux-amd64
make -j8 \
 -C cryptolib3/_linux-amd64 \
 -f ../Makefile \
 -I ..

mkdir -p _linux-amd64
make -j8 \
 -C _linux-amd64 \
 -f ../Makefile \
 ROOT=../.. \
 CFLAGS_USER=-fno-diagnostics-color \
 $ARGS
EOF
fi

# Build inside the container
podman exec $CONTAINER_NAME \
 bash -c "cd /src/fpassman && source ./build_$BUILD_TARGET.sh"
