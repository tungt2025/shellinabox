# Sử dụng Debian slim làm base
FROM debian:stable-slim

# Metadata
LABEL maintainer="tung"

# Cài các công cụ build cơ bản và git
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    autoconf \
    automake \
    libtool \
    pkg-config \
    git \
    ca-certificates \
    libssl-dev \
    libpam0g-dev \
    libjson-c-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Thư mục làm việc
WORKDIR /usr/src

COPY ./ /usr/src/shellinabox

WORKDIR /usr/src/shellinabox

# Tạo user không phải root để chạy service
RUN useradd -m -s /bin/false shellinabox || true

# Build từ source: autogen/configure/make
    RUN export LDFLAGS="-lssl -lcrypto" &&  autoreconf -i \
        && ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
        && make -j$(nproc) \
        && make install

# Tạo thư mục runtime và set quyền
RUN mkdir -p /var/lib/shellinabox /var/log/shellinabox \
    && chown -R shellinabox:shellinabox /var/lib/shellinabox /var/log/shellinabox

# Port mặc định của shellinabox (web terminal)
EXPOSE 4200

# Copy entrypoint script và cấp quyền thực thi
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Sử dụng entrypoint để tạo user động
ENTRYPOINT ["/entrypoint.sh"]
CMD ["shellinaboxd", "-t", "-s", "/:LOGIN"]
