#!/bin/bash 
set -e
echo "Bulding lib musl version..."
_image=$(docker build -q -f musl/Dockerfile "https://github.com/pregusia/wait-for-me.git#main")
echo "Transfering binaries from image to host filesystem..."
docker run --rm -v $(pwd):/copy "${_image}" cp /output/wait-for-me /copy/
echo "Cleaning up..."
docker image rm "${_image}"
