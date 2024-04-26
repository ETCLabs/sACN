FROM etc-docker.artifactory.etcconnect.com/etc/common-tech/clang-cmake-msan:latest
RUN apt-get update && apt-get install -y autopoint gettext flex bison

ENV CFLAGS="-nostdinc++ -fsanitize=memory -I/libcxx_msan/include -I/libcxx_msan/include/c++/v1"
ENV LDFLAGS="-stdlib=libc++ -fsanitize=memory -L/libcxx_msan/lib -rpath /libcxx_msan/lib"

# linux-util
RUN git clone --depth 1 https://github.com/util-linux/util-linux.git -b stable/v2.40 \
    && cd util-linux \
    && ./autogen.sh \
    && ./configure SOLIB_CFLAGS="$CFLAGS" SOLIB_LDFLAGS="$LDFLAGS" --disable-all-programs --enable-libuuid --libdir=/usr/lib/x86_64-linux-gnu \
    && make \
    && make install-strip \
    && rm -rf ../util-linux

ENV LD_LIBRARY_PATH='/libcxx_msan/lib/'