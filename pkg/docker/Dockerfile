FROM ubuntu:latest

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update
RUN apt-get install -qy git build-essential pkg-config
RUN apt-get install -qy libcairo2 \
                        libcairo2-dev \
                        libfontconfig-dev \
                        libfontconfig1 \
                        libfreetype-dev \
                        libfreetype6 \
                        libx11-6 \
                        libx11-dev \
                        libx11-xcb-dev \
                        libx11-xcb1 \
                        libxcb-ewmh-dev \
                        libxcb-ewmh2 \
                        libxcb-icccm4 \
                        libxcb-icccm4-dev \
                        libxcb-util-dev \
                        libxcb-util1 \
                        libxcb1 \
                        libxcb1-dev \
                        xcb

RUN git clone https://github.com/vl409/rieman.git

RUN cd rieman && \
    ./auto/configure --with-debug --with-tests && \
    make && echo "ALL GOOD"


