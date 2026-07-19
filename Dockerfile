FROM ubuntu:22.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake build-essential ca-certificates wget gnupg lsb-release software-properties-common git \
    && wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh \
    && chmod +x /tmp/llvm.sh && /tmp/llvm.sh 17 \
    && rm -rf /var/lib/apt/lists/* /tmp/llvm.sh

ENV CC=clang-17
ENV CXX=clang++-17

WORKDIR /src
COPY . .
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" && \
    cmake --build . -j"$(nproc)"

FROM ubuntu:22.04 AS runtime

WORKDIR /app
COPY --from=build /src/build/bin/app /app/app
COPY --from=build /src/sample_graph /app/sample_graph

WORKDIR /data
ENTRYPOINT ["/app/app"]
CMD ["--input", "/app/sample_graph/edges.csv", "--output", "/data/result.csv", "--eps", "1e-6", "--max-iters", "100"]