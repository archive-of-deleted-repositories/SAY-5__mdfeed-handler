# Multi-stage build. Final image runs the handler.
# Builder stage: full toolchain. Runtime stage: glibc + the binary only.

FROM ubuntu:22.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY src/ src/
COPY tests/ tests/
COPY bench/ bench/

RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DMDFEED_BUILD_TESTS=OFF && \
    cmake --build build -j

FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/mdfeed_handler  /usr/local/bin/mdfeed_handler
COPY --from=builder /src/build/mdfeed_simulator /usr/local/bin/mdfeed_simulator
COPY --from=builder /src/build/loopback_bench   /usr/local/bin/loopback_bench

USER 65534:65534

ENTRYPOINT ["/usr/local/bin/mdfeed_handler"]
CMD ["--host=0.0.0.0", "--port=42424"]
