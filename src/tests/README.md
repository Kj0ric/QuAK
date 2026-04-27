# Tests And Probes

The test tree separates registered regression coverage from optional
investigation tools.

## Registered Tests

- `sanity_tests/`: fast structural and API-oriented checks for flattening,
  synchronization, pseudo-determinization, and helpers.
- `correctness_tests/`: semantic regression tests for emptiness,
  universality, final-aware accepted-domain behavior, specialized
  extremal/monotone flattening, and known bug fixtures.
- `samples/tests/correctness/`: fixtures used by registered correctness tests
  and selected probes.
- `samples/tests/sanity/`: fixtures used by registered sanity tests.

Registered tests are listed in the top-level `CMakeLists.txt`, build with:

```bash
cmake --build build --target tests -j
```

and run with:

```bash
ctest --test-dir build --output-on-failure
```

Use `ctest --test-dir build -N` for the current registered test list.

## Optional Tools

- `probes/`: backend comparison and diagnostic programs. Some are still built
  by the `experiments` target, but they are not registered as CTest tests.
- `benchmarks/`: benchmark harness sources. They are not built unless a CMake
  target explicitly enables them.

Promote a probe case into `correctness_tests/` when it captures a compact,
semantic regression worth running on every test pass.
