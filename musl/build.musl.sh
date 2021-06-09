#!/bin/bash 
set -e
echo "Bulding lib musl version..."
_image=$(docker build -q -f musl/Dockerfile "https://github.com/pregusia/wait-for-me")
echo "Transfering binaries from image to host filesystem..."
mkdir -p bin || true
docker run --rm -v $(pwd)/bin:/copy "${_image}" cp -r /output/. /copy/
echo "Cleaning up..."
docker image rm "${_image}"
