# Analysis Area

This directory is for research notes, cleanup plans, evaluation logs, reports,
and one-off analysis tools.

Use this area for:

- design or implementation plans that are not user-facing documentation
- backend comparison notes and raw investigation outputs
- scripts or source snippets used to inspect a specific hypothesis
- reports moved out of `src/tests/` once they are no longer compiled

Do not place production library code here. Production C++ belongs under `src/`;
registered tests belong under `src/tests/sanity_tests/` or
`src/tests/correctness_tests/`; optional compiled probes belong under
`src/tests/probes/` unless they are truly standalone analysis artifacts.

Generated benchmark outputs should stay in `results/` or in a dated/raw
analysis subdirectory when the files are part of an investigation record.
