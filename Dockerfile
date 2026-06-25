FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc-i686-linux-gnu \
    binutils-i686-linux-gnu \
    make \
    dosfstools \
    mtools \
    sparse \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
CMD ["make"]

