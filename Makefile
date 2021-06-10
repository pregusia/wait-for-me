.PHONY: musl

all:
	cc -O3 -s -o wait-for-me main.cc

musl:
	musl/build.musl.sh

clean:
	rm -f wait-for-me


