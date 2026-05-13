FROM ubuntu:22.04

RUN apt update && apt install -y \
    g++ \
    cmake \
    make \
    libpq-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN mkdir build && cd build && cmake .. && make

EXPOSE 8080

CMD ["./build/server"]

