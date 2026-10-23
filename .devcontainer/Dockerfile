# Base image: Ubuntu x86_64
FROM --platform=linux/amd64 ubuntu:22.04

ENV TZ=Asia/Seoul LANG=ko_KR.UTF-8 LANGUAGE=ko_KR.UTF-8

# Install dependencies
RUN export DEBIAN_FRONTEND=noninteractive && \
    apt-get update \
    # && echo "tzdata tzdata/Areas select Asia" | debconf-set-selections \
    # && echo "tzdata tzdata/Zones/Asia select Seoul" | debconf-set-selections \
    && apt-get install -y \
    locales tzdata \
    build-essential \
    gcc \
    gdb \
    vim \
    git \
    sudo \
    qemu-system-x86 \
    python3 \
    python3-pip \
    && locale-gen ko_KR.UTF-8 && update-locale LANG=ko_KR.UTF-8 
# && apt-get clean \
# && rm -rf /var/lib/apt/lists/*

# Team convention tools, pinned so every editor and `make lint` agree.
# clangd 21 formats with the same engine as clang-format 21.
RUN pip3 install --no-cache-dir \
    clangd==21.1.8 clang-format==21.1.8 clang-tidy==21.1.6

RUN useradd -m -s /bin/bash jungle && usermod -aG sudo jungle
RUN echo "jungle ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/jungle

USER jungle
WORKDIR /home/jungle


# Clone the repository
# RUN git clone https://github.com/casys-kaist/pintos-kaist /home/jungle/pintos

# Set default shell for the `jungle` user