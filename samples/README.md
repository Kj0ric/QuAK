# Sample Automata

This directory contains the reviewer-facing automata shipped with the artifact.
All non-nested automata kept here declare final states explicitly.

## Curated Non-Nested Samples

- `A.txt`
- `B.txt`
- `non-nested/with_final_states.txt`

Older non-nested examples without `final:` declarations were removed because the
current CLI interprets non-nested queries using Buchi acceptance over declared
final states.

## Nested Samples

- `nested/`: small hand-written nested automata used by sanity tests and for
  exploration.
- `pseudo_determ/`: small pseudo-determinization examples.
- `testChildReturnValues.txt` and `testMonitorConstruction.txt`: hand-written
  nested fixtures for library behavior checks.

## Registered Test Fixtures

- `tests/sanity/`: structural sanity-test inputs.
- `tests/correctness/`: semantic regression inputs used by CTest and the smoke
  test.

The registered C++ tests live under `src/tests/`; the automata fixtures live
here so artifact inputs are centralized under `samples/`.

## Paper Experiment Inputs

These generated directories are required by `experiment.py` and should remain
checked in for artifact reproducibility:

- `generated_response_time_1`
- `generated_response_time_2`
- `generated_response_time_3`
- `generated_resource_consumption_1`
- `generated_resource_consumption_2`
