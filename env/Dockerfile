FROM ubuntu:22.04

RUN apt update && \
    apt install -y software-properties-common && \
    add-apt-repository universe && \
    apt update && \
    apt install -y \
    build-essential \
    bison \
    flex \
    libgmp-dev \
    libmpc-dev \
    libmpfr-dev \
    texinfo \
    gcc \
    nasm \
    mtools \
    python3 \
    python3-sh \
    scons \
    python3-parted \
    dosfstools \
    parted \
    git \
    wget \
    curl && \
    apt clean && rm -rf /var/lib/apt/lists/*

WORKDIR /valkyrie_os

# Copy project files into container
COPY . /valkyrie_os/

# Build i686-elf cross-toolchain at /os_toolchain
# RUN mkdir -p /os_toolchain && \
#    bash /valkyrie_os/scripts/base/toolchain.sh /os_toolchain && \
#    find . -name "*.o" -delete && \
#    find . -name "*.a" -delete && \
#    rm -rf binutils-* gcc-* *.tar.xz

# Set up environment to use the toolchain
ENV PATH="/os_toolchain/i686-elf/bin:${PATH}"

CMD ["/bin/bash"]