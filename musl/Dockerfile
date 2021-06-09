FROM alpine:3
RUN apk --no-cache add build-base
COPY . /build/
WORKDIR /build
#
RUN mkdir /output &&\
 gcc -o /output/wait-for-me.musl -O3 -s main.cc &&\
 gcc -o /output/wait-for-me-static.musl -O3 -s -static main.cc
