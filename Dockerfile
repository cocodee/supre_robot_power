FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    ninja-build \
    pkg-config \
    python3 \
    python3-dev \
    python3-pip \
    pybind11-dev \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["bash"]
