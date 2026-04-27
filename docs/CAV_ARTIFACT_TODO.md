# CAV Artifact TODO

This checklist tracks the remaining artifact work for the CAV 2026 submission.
It is based on the current repository state and the open items in Harun's list.

## Immediate Submission Package

- [ ] Build the final Docker image from a clean checkout.
  - Command:
    ```bash
    docker build -t quak-nqa .
    ```
  - Confirm that the Docker build runs the full CTest suite in the builder stage.
  - Save the build log or record the commit/state used for the final image.

- [ ] Fix the Docker smoke-test command mismatch.
  - Current issue: `.github/workflows/docker-build.yml` runs:
    ```bash
    docker run --rm quak-nqa /quak/scripts/smoke-test.sh --quick
    ```
  - Current script behavior: `scripts/smoke-test.sh` rejects all arguments.
  - Choose one fix:
    - Remove `--quick` from the workflow, or
    - Teach `scripts/smoke-test.sh` to accept `--quick` as an alias for the existing smoke test.
  - Re-run the Docker smoke test after the fix.

- [ ] Fix runtime Docker contents needed by `experiment.py`.
  - Current issue: the runtime image copies `experiment.py`, but `experiment.py` imports:
    ```text
    src/archived/experiment_skip_oot_oom.py
    ```
  - Add that helper file to the runtime image, or move/refactor the helper into a reviewer-facing script location.
  - Verify inside the container:
    ```bash
    docker run --rm quak-nqa python3 /quak/experiment.py --help
    ```

- [ ] Decide whether the runtime image should include `experiment_small.py`.
  - If reviewers are expected to run the small experiment subset, copy it into the image.
  - Document it in `docs/AE_README.md`.
  - Verify inside the container:
    ```bash
    docker run --rm quak-nqa python3 /quak/experiment_small.py --help
    ```

- [ ] Decide whether the runtime image should include table-regeneration assets.
  - Candidate files:
    - `results/csv_to_latex_figures.py`
    - `results/paper/*.csv`
    - `results/paper/benchmark_tables.tex`
    - `results/paper/benchmark_tables.pdf`
  - If included, document exactly which outputs reviewers should compare.

- [ ] Save the final Docker image.
  - Example:
    ```bash
    docker save quak-nqa | gzip > quak-nqa-docker-image.tar.gz
    ```
  - Confirm the load command works on a clean machine:
    ```bash
    docker load < quak-nqa-docker-image.tar.gz
    docker run --rm quak-nqa /quak/scripts/smoke-test.sh
    ```

- [ ] Create the final artifact zip.
  - Include:
    - Source code needed to build from scratch.
    - `quak-nqa-docker-image.tar.gz`.
    - `docs/AE_README.md`.
    - `LICENSE`.
    - Relevant `samples/`.
    - Relevant `results/paper/`.
    - Scripts needed for experiments and table generation.
  - Exclude:
    - Local build directories.
    - Temporary logs unless intentionally included.
    - Agent notes and local-only planning files.

- [ ] Compute SHA256 checksums.
  - At minimum:
    ```bash
    sha256sum quak-cav26-artifact.zip quak-nqa-docker-image.tar.gz > SHA256SUMS
    ```
  - Include `SHA256SUMS` in the upload if allowed.

- [ ] Upload to Zenodo.
  - Upload the final zip.
  - Record the version DOI.
  - Prefer the version DOI over the concept DOI in the submission metadata.
  - After upload, download the archive once and verify the checksum.

## Smoke Test

- [ ] Decide whether to keep the current shell smoke test only, or also add Harun's requested CTest smoke test.
  - Current state:
    - `scripts/smoke-test.sh` exists.
    - `make smoke-test` exists through the top-level `CMakeLists.txt`.
    - No `src/tests/correctness_tests/test_smoke.cpp` appears registered in CTest.
  - If matching Harun's list exactly matters, add `src/tests/correctness_tests/test_smoke.cpp` and register it with `add_quak_test`.

- [ ] Verify the shell smoke test from a clean native build.
  - Commands:
    ```bash
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    cmake --build build --target experiments -j
    cp build/quak-nested ./quak-nested
    cp build/quak-experiment-single ./quak-experiment-single
    bash scripts/smoke-test.sh
    ```
  - Expected ending:
    ```text
    SMOKE PASSED -- 16/16 checks, 0s wall
    ```
  - Record actual wall time on the test machine.

- [ ] Verify the smoke test through Docker on Ubuntu.
  - Commands:
    ```bash
    docker build -t quak-nqa .
    docker run --rm quak-nqa /quak/scripts/smoke-test.sh
    ```
  - Confirm the smoke test passes from the runtime image, not only from the source tree.

- [ ] Verify the smoke test through Docker on macOS.
  - Commands:
    ```bash
    docker load < quak-nqa-docker-image.tar.gz
    docker run --rm quak-nqa /quak/scripts/smoke-test.sh
    ```
  - Confirm Docker architecture compatibility, especially on Apple Silicon.
  - If the artifact is x86_64-only, document that requirement clearly in `docs/AE_README.md`.

- [ ] Verify CTest from a clean native build.
  - Commands:
    ```bash
    cmake --build build --target tests -j
    ctest --test-dir build --output-on-failure
    ```
  - Confirm all registered tests pass.
  - Record the number of tests and total runtime.

## AE README

- [ ] Fill the badge justification section in `docs/AE_README.md`.
  - Explain why the artifact is **Available**.
  - Explain why the artifact is **Reusable**.
  - Mention that Reusable subsumes Functional, if following the CAV template wording.

- [ ] Fill resource requirements.
  - Replace TODOs for:
    - RAM.
    - CPU cores.
    - Disk.
    - Full-review time.
  - Make requirements distinguish:
    - Smoke test.
    - Small experiment subset.
    - Full paper experiment reproduction.
    - Native build from source.
    - Docker-only use.

- [ ] Complete Docker load/run instructions.
  - Include exact commands:
    ```bash
    docker load < quak-nqa-docker-image.tar.gz
    docker run --rm quak-nqa /quak/scripts/smoke-test.sh
    docker run --rm -it quak-nqa
    ```
  - Include the expected smoke-test output.
  - Mention where the reviewer lands inside the container (`/quak`).

- [ ] Complete native build instructions.
  - Include commands for:
    - Configure.
    - Build.
    - Build tests.
    - Run CTest.
    - Build experiments.
    - Run smoke test.

- [ ] Complete the full-review section.
  - Include exact commands for:
    - Running the small representative experiments.
    - Running all configured experiments.
    - Regenerating LaTeX tables from CSVs.
    - Comparing regenerated outputs with `results/paper`.
  - Clearly state which commands are expected to finish quickly and which may take hours.

- [ ] Fill known limitations.
  - Replace the remaining TODO.
  - Keep Windows/MSVC limitation if still accurate.
  - Add architecture limitations if Docker is x86_64-only.
  - Mention any known timeouts or out-of-memory cells in paper tables.

- [ ] Add troubleshooting notes.
  - Docker permission errors.
  - Missing Python.
  - Missing LaTeX tools if PDF regeneration is optional.
  - Expected behavior for timeout or OOM cells in the benchmark tables.

## CI And Cross-Platform Checks

- [ ] Verify GitHub Actions CI for regular build/test.
  - Workflow:
    - `.github/workflows/ci-build-and-test.yml`
  - Confirm it builds tests before running `ctest`.
  - Confirm branch filters are correct for the submission branch.

- [ ] Verify GitHub Actions CI for Docker.
  - Workflow:
    - `.github/workflows/docker-build.yml`
  - Fix the `--quick` mismatch before relying on this workflow.
  - Confirm Docker build includes CTest.
  - Confirm Docker runtime smoke test passes.

- [ ] Check Ubuntu locally.
  - Native build.
  - CTest.
  - Docker build.
  - Docker smoke test.
  - Small experiment subset.

- [ ] Check macOS locally.
  - Native build if supported.
  - CTest.
  - Docker load.
  - Docker smoke test.
  - Note any Apple Silicon caveats.

## Licensing And Metadata

- [ ] Confirm all included third-party material is compatible with the license.
  - Source code.
  - Sample inputs.
  - Generated result files.
  - Docker base image references.

- [ ] Add final artifact metadata to `docs/AE_README.md`.
  - Artifact version.
  - Zenodo DOI.
  - SHA256 checksum.
  - Contact email if required by the CAV template.

## Final Pre-Submission Checklist

- [ ] Clean checkout builds natively.
- [ ] Clean checkout passes CTest.
- [ ] Docker image builds from clean checkout.
- [ ] Docker image passes smoke test.
- [ ] Docker image can run `experiment.py --help`.
- [ ] Docker image can run `experiment_small.py --help` if included.
- [ ] AE README has no TODO markers.
- [ ] Packaged artifact zip has no accidental build artifacts.
- [ ] Artifact zip includes Docker image tarball.
- [ ] Artifact zip includes source, docs, license, samples, and paper results.
- [ ] SHA256 checksum recorded.
- [ ] Zenodo upload completed.
- [ ] Zenodo DOI recorded in the submission.
- [ ] Downloaded Zenodo archive checksum matches the local checksum.
