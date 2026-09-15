#!/usr/bin/env bash
set -euo pipefail

# Build dependencies (Ubuntu/Debian, install once with sudo):
#   sudo apt-get install -y build-essential libsdl2-dev libopenal-dev \
#       libgl1-mesa-dev libjpeg-dev pkg-config

PREMAKE_URL="https://github.com/premake/premake-core/releases/download/v5.0.0-beta1/premake-5.0.0-beta1-linux.tar.gz"

# Download Premake 5 beta1 if it is not already present.
if [ ! -x ./premake5 ]; then
	curl -fL "$PREMAKE_URL" -o premake5.tar.gz
	tar xf premake5.tar.gz
	rm -f premake5.tar.gz
fi
PREMAKE="$(pwd)/premake5"

# The project carries its PsyCross changes as a patch; apply it before building.
REPO_ROOT="$(pwd)"
PATCH="$REPO_ROOT/patches/psycross/developer-overlay.patch"
if [ ! -e src_rebuild/PsyCross/.git ]; then
	echo "PsyCross submodule is not initialised. Run: git submodule update --init --recursive" >&2
	exit 1
fi
if [ ! -f "$PATCH" ]; then
	echo "PsyCross patch not found: $PATCH" >&2
	exit 1
fi
if git -C src_rebuild/PsyCross apply --reverse --check "$PATCH" >/dev/null 2>&1; then
	echo "PsyCross developer-overlay patch is already applied."
else
	git -C src_rebuild/PsyCross apply "$PATCH"
fi

# Configure
cd src_rebuild
"$PREMAKE" gmake2
"$PREMAKE" vscode
cd build

echo "Generated Linux makefiles in $(pwd)"
echo "Build with: make -j\"\$(nproc)\" config=release_dev_x64"
