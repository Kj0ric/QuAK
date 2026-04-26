# Archived Material

This directory contains files kept for reference but intentionally excluded
from normal builds.

Contents:

- `legacy_sources/`: historical source snapshots and comparison baselines.
- `fragments/`: extracted backend fragments, replacement blocks, old
  configuration snapshots, and other preserved implementation notes.
- `experiments_oblbag/`: archived experiment material for obligation-bag work.
- `test_split_final_differential.cpp`: randomized split-final differential
  stress test. It is superseded in the registered suite by explicit
  split-final regression cases in `src/tests/correctness_tests/`.
- `test_sanity_all.cpp`: old umbrella sanity executable. Its checks are
  covered by the focused sanity executables; the unique parser `final: all`
  check was moved to `src/tests/sanity_tests/test_auxiliary_functions.cpp`.
- `test_universality_with_final.cpp`: former standalone accepted-domain
  universality/Forklift regression test. Its active checks now live in
  `src/tests/correctness_tests/test_universality_correctness.cpp`.

Active production code lives directly under `src/`. Do not add archived files
to CMake targets unless you are deliberately creating a one-off comparison or
investigation tool.

When moving material here, prefer a short README or note explaining why it is
archived and what active implementation supersedes it.
