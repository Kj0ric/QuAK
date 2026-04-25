# CAV 2026 Artifact

**Paper title:** Extending QuAK with Nested Quantitative Automata

**Claimed badges:** Available + Reusable

## Justification for the badges

- **Reusable** (subsumes Functional): TODO

## Requirements

| Resource | Requirement |
|----------|-------------|
| RAM | TODO |
| CPU cores | TODO |
| Disk | TODO |
| Time (smoke test) | < 1 minute (build ~2 min + ctest ~5 sec) |
| Time (full review) | TODO |

**External connectivity:** NO

---

## Smoke Test

This smoke test builds the tool and verifies that it installs correctly and
runs without error. It exercises each major decision procedure on a small
representative input to confirm the binary starts, reads input, and produces
output in the expected format. It does not verify paper claims; that is the
goal of the full review.

### Using Docker (recommended)

```bash
docker load < quak-nqa-image.tar.gz
docker run --rm quak-nqa /quak/scripts/smoke-test.sh --quick
```

Expected output ends with:
```
SMOKE PASSED -- 16/16 checks, 0s wall
```

For an interactive shell inside the container:

```bash
docker run --rm -it quak-nqa
```

You are placed in `/quak`. You can then run `scripts/smoke-test.sh` manually or
explore the tool interactively (e.g. `./quak-nested`, `ls samples/`).

### Without Docker (requires C++17 compiler + CMake >= 3.9)

```bash
tar -xzf quak-nqa.tar.gz
cd quak-nqa
```

**Recommended: one-command smoke test:**

```bash
bash scripts/smoke-test.sh --quick
```

This builds all required binaries automatically if needed, then runs the
representative test suites (nested + non-nested CLI). Expected output:

```
==> [1/4] Checking binaries...
  PASS [quak-nested exists and is executable]
  PASS [quak-experiment-single exists and is executable]
==> [2/4] Checking input files...
  PASS [test-inputs/ directory present]
  PASS [samples/ directory present]
  PASS [samples/A.txt present (non-nested fixture)]
  PASS [generated benchmark inputs present (271 files)]
==> [3/4] Checking Python environment and experiment script...
  PASS [python3 is available]
  PASS [experiment.py loads and accepts --help]
==> [4/4] Checking decision procedures (one per flattening path)...
  PASS [flatten_regular      LimSupAvg/Max_f   (non-empty, expect = 1)]
  ...
SMOKE PASSED -- 16/16 checks, 0s wall
```

**Alternative: full test suite (all 16 tests, ~10 s):**

```bash
bash scripts/smoke-test.sh --full
```

**Manual steps (if you prefer not to use the wrapper script):**

Step 1 - Build:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
cmake --build build --target tests -j4
```

Step 2 - Run:
```bash
ctest --test-dir build --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 16`

If any test fails, the `--output-on-failure` flag prints the failing assertion.
Please include the failing test name and output in your review.

---

## Full Review

TODO

---

## Known Limitations

- **Windows/MSVC:** not officially supported (pre-existing VLA compatibility
  issue); Linux and macOS are fully supported and tested in CI.
- TODO
