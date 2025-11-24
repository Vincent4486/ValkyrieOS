# SPDX-License-Identifier: AGPL-3.0-or-later
FROM archlinux:latest

# Install build dependencies using pacman and pip for a few python bindings
RUN pacman -Syu --noconfirm --needed && \
        pacman -S --noconfirm --needed \
            base-devel \
            bison \
            flex \
            gmp \
            mpfr \
            libmpc \
            texinfo \
            gcc \
            nasm \
            mtools \
            python \
            python-sh \
            python-pyparted \
            scons \
            dosfstools \
            parted \
            git \
            wget \
            curl && \
        pacman -Scc --noconfirm

WORKDIR /valkyrie_os

# Copy project files into container
COPY . /valkyrie_os/

# Build i686-elf cross-toolchain at /os_toolchain (optional)
# RUN mkdir -p /os_toolchain && \
#    bash /os_toolchain/scripts/base/toolchain.sh /os_toolchain && \
#    cd /os_toolchain && \
#    find . -name "*.o" -delete && \
#    find . -name "*.a" -delete && \
#    rm -rf binutils-* gcc-* *.tar.xz

# Set up environment to use the toolchain
ENV PATH="/os_toolchain/i686-elf/bin:${PATH}"

CMD ["/bin/bash"]
