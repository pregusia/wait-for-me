FROM alpine:3
RUN apk --no-cache add build-base
COPY . /build/
WORKDIR /build
#
RUN mkdir /output &&\
 gcc -o /output/wait-for-me -O3 -s -static main.c
