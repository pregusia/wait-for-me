.PHONY: musl

all:
	cc -O3 -s -o wait-for-me main.c

musl:
	musl/build.musl.sh

clean:
	rm -f wait-for-me


