# =============================================================================
# Stage 1 — builder
# Compiles QuAK-NQA, builds all test executables, and runs the full test suite.
# If any test fails, the image fails to build — tests cannot be skipped.
# =============================================================================
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -y update && \
    apt-get install -y --no-install-recommends \
        g++ \
        make \
        cmake \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /opt/quak-build
COPY . .

RUN cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_IPO=ON \
        -DENABLE_SCC_SEARCH_OPT=ON && \
    cmake --build build -j$(nproc) && \
    cmake --build build --target tests -j$(nproc) && \
    cmake --build build --target experiments -j$(nproc)

RUN ctest --test-dir build --output-on-failure

# =============================================================================
# Stage 2 — runtime
# Copies only the built binaries and reviewer-facing files.
# The compiler, build system, and intermediate objects are discarded.
# =============================================================================
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -y update && \
    apt-get install -y --no-install-recommends \
        python3 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /quak

COPY --from=builder /opt/quak-build/build/quak-nested                    ./quak-nested
COPY --from=builder /opt/quak-build/build/quak-experiment-single          ./quak-experiment-single
COPY --from=builder /opt/quak-build/samples/                              ./samples/
COPY --from=builder /opt/quak-build/docs/AE_README.md                     ./AE_README.md
COPY --from=builder /opt/quak-build/docs/CLI.md                           ./docs/CLI.md
COPY --from=builder /opt/quak-build/docs/assumptions.md                   ./docs/assumptions.md
COPY --from=builder /opt/quak-build/scripts/smoke-test.sh                 ./scripts/smoke-test.sh
COPY --from=builder /opt/quak-build/experiment.py                         ./experiment.py
COPY --from=builder /opt/quak-build/LICENSE                               ./LICENSE

ENV PATH="/quak:${PATH}"

ENTRYPOINT ["/bin/bash"]
