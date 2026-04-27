# Changelog

## AE Packaging (PR #8)

This set of changes prepares the artifact for the Artifact
Evaluation submission. The goal was to make the artifact easy for reviewers to
set up and verify, and to protect against packaging mistakes going unnoticed.

### What was added

**Smoke test script (`scripts/smoke-test.sh`)**

Before this PR, there was no single command a reviewer could run to check that
everything works. Now there is. Running `bash scripts/smoke-test.sh`
does four things in order:

1. Checks that the compiled binaries (`quak-nested` and `quak-experiment-single`)
   are actually present and executable.
2. Checks that all the input files are there -- the test fixtures, the sample
   automata, and the pre-generated benchmark inputs that the experiment script
   needs. Without these, `experiment.py` would silently run zero instances.
3. Checks that Python is installed and that `experiment.py` starts up without
   crashing. This catches the most common setup failure before a reviewer wastes
   time trying to run experiments.
4. Runs `quak-nested` on a small input once per flattening path, to confirm the 
   binary actually works on the current machine.

The script runs the four checks directly against the installed binaries.

A `make smoke-test` target was also added to CMakeLists.txt as a shortcut for
developers working in the build directory.

**Dockerfile rewrite**

The old Dockerfile built the tool but never ran the tests, and had no entry
point for reviewers. The new one does three things differently:

- It uses two build stages. The first stage (the "builder") compiles everything
  and runs all 16 tests. If any test fails, the image fails to build -- you
  cannot accidentally ship a broken image. The second stage copies only the
  compiled binaries and input files into a clean, small image (about 52 MB).
- It sets up the container so that `docker run --rm -it quak-nqa` drops the
  reviewer into a shell at `/quak` with the binaries ready to use.
- A `.dockerignore` file was added so that local build artifacts, git history,
  and working files (like `TODO.md`) are not included in the image.

**Docker CI workflow (`.github/workflows/docker-build.yml`)**

Since the Docker image is the artifact we submit, a broken Dockerfile is a
submission failure. This new workflow runs on every push to `playground` and
does two things: builds the Docker image (which runs the tests inside), then
runs the smoke test against the built image. If either step fails, the push is
flagged immediately.

**AE README converted to Markdown (`docs/AE_README.md`)**

The AE README was plain text following the chairs' template format. It has been
converted to Markdown so it renders properly on GitHub and is easier to read
and edit.

**Input directory documentation (`samples/tests/correctness/README.md`)**

The correctness test input directory had 37 files with no explanation of what
they are or why they exist. A README was added that documents the naming
convention and describes each file. It also explains why `baseline_det.txt`
looks similar to `tc01_simple_det.txt` in `samples/tests/sanity/` -- they
serve different test families and are not accidental duplicates.

### What was not changed

- The test suite itself (`src/tests/`) -- no new test cases were added. The
  smoke test is a wrapper around existing tests, not new logic.
- The `FULL REVIEW` section of `docs/AE_README.md` -- that section covers
  experiment reproduction and is being handled separately.
