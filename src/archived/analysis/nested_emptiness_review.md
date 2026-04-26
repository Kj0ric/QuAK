# Nested Emptiness Review

This file records findings from the ongoing correctness and efficiency review of the nested automata emptiness procedures.

## Scope

- Entry point: `NestedAutomaton::isNonEmpty`
- Follow-up targets:
  - child-return helpers used by `flatten_regular`
  - `flatten_regular`
  - monotone `SumPlus`/`SumMinus` flattenings
  - monotone `Min_f`/`Max_f` flattenings
  - `SumMinus + LimAvg` preprocessing pipeline

## Severity Rubric

- `high`: direct wrong answer on a supported CLI-visible query, backed by a small runtime witness
- `medium`: real correctness or robustness issue, but niche, masked by defaults, abort-only, or otherwise narrower in practical reach
- `low`: API/doc mismatch, performance note, static risk without a clean end-to-end witness, or a summary/meta finding

## Findings

### 2026-04-16: Dispatcher review (`NestedAutomaton::isNonEmpty`)

#### 1. Correctness bug: `c_bound` computed from the wrong automaton

- Location: `src/NestedAutomaton.cpp:6995-7007`
- Severity: medium
- Status: open

In the `SumMinus + LimAvg` preprocessing, the code computes `c_bound` from the wrong automaton in every branch that first transforms `this`:

- deterministic-but-incomplete branch:
  - builds `det_nwa = makeCompleteNested(&complete_flags)`
  - then still computes `c_bound = compute_c_bound(this)`
- nondeterministic branch:
  - builds `det_nwa = determinizeWithMacroAlphabet()`
  - then still computes `c_bound = compute_c_bound(this)`

Why this matters:

- `makeCompleteNested` can add sink states to the parent and children.
- `determinizeWithMacroAlphabet` can increase the effective deterministic child-state space before synchronization.
- `compute_c_bound` is explicitly size-based over the deterministic pre-synchronization automaton.
- Using the smaller pre-transformation automaton can underestimate the multiplicity bound passed to `flatten_Avg_SumMinus`.

This can cause false negatives both:

- on incomplete deterministic inputs
- and on nondeterministic inputs whose deterministic preprocessing increases the relevant configuration space

Relevant code:

- `src/NestedAutomaton.cpp:1778-1985` (`makeCompleteNested`)
- `src/NestedAutomaton.cpp:2710-2770` (`compute_c_bound`)
- `src/NestedAutomaton.cpp:6991-7010` (`isNonEmpty` branch for `SumMinus + LimAvg`)

#### 2. API contract hole: `SumB` bound is not validated in the public nested API

- Location: `src/NestedAutomaton.h:64`, `src/NestedAutomaton.cpp:7027-7028`
- Severity: low
- Status: open

The public `NestedAutomaton::isNonEmpty` signature defaults `bound` to `-1`, but the `SumB` branch forwards that value unchecked into `flatten_regular(finVal, bound)`.

The downstream `SumB` logic uses `bound` arithmetically when computing and clipping return values, so the API currently accepts a semantically invalid call shape and does not fail fast.

CLI parsing may prevent this at the command line, but the C++ API does not enforce the requirement.

Relevant code:

- `src/NestedAutomaton.h:64`
- `src/NestedAutomaton.cpp:655-760` (`computeSumBReturnValues`)
- `src/NestedAutomaton.cpp:1523-1705` (`flatten_regular`)
- `src/NestedAutomaton.cpp:7027-7028` (`isNonEmpty`)

#### 3. Efficiency concern: `SumPlus + LimSupAvg` duplicates work on negative fast-path outcomes

- Location: `src/NestedAutomaton.cpp:6942-6989`
- Severity: low
- Status: open

For `SumPlus + LimSupAvg`, the dispatcher first constructs and checks a full `flatten_SumPlusMinus_Sup` reduction. If that does not prove non-emptiness, it discards the result and builds a second, unrelated reduction via `flatten_regular(SumB, theoretical_bound)`.

This is not a correctness bug by itself, but it matches the current benchmark pain point:

- `results/paper/response_limsupavg_sumplus_emptiness.csv:2` records a timeout.

This path should remain a priority during the later efficiency pass.

#### 16. Correctness bug: `SumPlus + LimSupAvg` fast path is unsound on fractional weights

- Location: `src/NestedAutomaton.cpp:6945-6968`
- Severity: high
- Status: open

The `SumPlus + LimSupAvg` dispatcher computes a “theoretical bound” as

- `tb * max_weight * |Q_m|`

where `max_weight` is obtained via:

- `this->getChild(i)->getMaxDomain().to_uint()`

That truncates positive fractional maxima downward. The code then uses the resulting underestimated bound in a fast-path check and returns `true` immediately if a `LimSup`-style threshold-crossing run exists:

- `Automaton* fastFlat = this->flatten_SumPlusMinus_Sup(SumPlus, theoretical_bound);`
- `bool fastResult = fastFlat->isNonEmpty_withFinal(LimSup, weight_t(1));`
- `if (fastResult) return true;`

If the bound is underestimated, this can classify a merely bounded sequence of child returns as “beyond the lemma bound” and return `true` for **arbitrarily high user thresholds** without ever consulting `x`.

Concrete runtime evidence:

- Witness file:
  - `/tmp/sumplus_limsupavg_fractional_fastpath.txt`
  - contents:
    ```text
    @PARENT
    a : 1, q0 -> q0

    @CHILD 0

    @CHILD 1
    final: f
    a : 1.9, s0 -> s1
    a : 1.9, s1 -> f
    ```
- Semantic reasoning:
  - on `a^\omega`, each invocation of child `1` returns `1.9 + 1.9 = 3.8`
  - the induced return sequence is bounded by `3.8`
  - so `LimSupAvg` is `3.8`
  - therefore thresholds `4` and `10` should both be false

Observed results:

- `./quak-nested /tmp/sumplus_limsupavg_fractional_fastpath.txt non-empty LimSupAvg SumPlus 10` -> `1`
- `./quak-nested /tmp/sumplus_limsupavg_fractional_fastpath.txt non-empty LimSupAvg SumPlus 4` -> `1`
- `./quak-nested /tmp/sumplus_limsupavg_fractional_fastpath.txt non-empty LimSupAvg SumPlus 3` -> `1`

Why this witness hits the bug:

- child size is `3`
- `getMaxDomain()` is `1.9`
- `to_uint()` truncates that to `1`
- with one parent state, the computed fast-path bound is `3 * 1 * 1 = 3`
- but the true child return is `3.8 > 3`, so the fast path fires and returns `true`
- that early return is unsound for thresholds above the actual `LimSupAvg` value

#### 17. Correctness bug: dispatcher returns `true` for `SumPlus` and `x <= 0` without checking acceptance

- Location: `src/NestedAutomaton.cpp:6936-6939`
- Severity: high
- Status: open

At the top of `NestedAutomaton::isNonEmpty`, the dispatcher performs this unconditional short-circuit:

- `if (x <= 0 && finVal == SumPlus) return true;`

That is only sound if an accepting nested run is guaranteed to exist. The repo semantics do **not** guarantee that:

- parent acceptance still requires an accepting infinite parent run
- and the README explicitly requires infinitely many non-silent child invocations for parent acceptance (`README.md:384`)

So `SumPlus >= 0` is not enough by itself. If the nested automaton has no accepting run at all, emptiness must remain false.

Concrete runtime evidence:

- Witness file:
  - `/tmp/sumplus_trivial_accept_bug.txt`
  - contents:
    ```text
    @PARENT
    a : 0, q0 -> q0

    @CHILD 0
    ```
- Semantic reasoning:
  - the parent only takes dummy/silent transitions
  - no real child is ever invoked
  - under the documented acceptance condition, there is no accepting nested run

Observed results:

- `./quak-nested /tmp/sumplus_trivial_accept_bug.txt non-empty Sup SumPlus 0` -> `1`
- `./quak-nested /tmp/sumplus_trivial_accept_bug.txt non-empty LimSupAvg SumPlus 0` -> `1`
- `./quak-nested /tmp/sumplus_trivial_accept_bug.txt non-empty Inf SumPlus -1` -> `1`

All of those should be false, because the automaton is empty regardless of threshold.

## Coverage Notes

- Current tests cover `makeCompleteNested` in isolation and `LimSupAvg + SumMinus` correctness separately.
- I have not yet found a test that specifically exercises the deterministic-but-incomplete `SumMinus + LimAvg` integration path where finding 1 occurs.

### 2026-04-16: Child-return helpers and `flatten_regular`

#### 4. Correctness / spec mismatch: `flatten_regular` does not handle actual parent `SILENT` transitions

- Location: `src/NestedAutomaton.cpp:1674-1705`
- Severity: medium
- Status: open

Inside `flatten_regular`, the code classifies a parent step as silent only when `pw == 0`. That matches the project convention that child index `0` means “no real child”, but it does **not** match the documented parser-level semantics for the `SILENT` keyword, which is represented as `weight_t(SILENT)`.

Consequences:

- A parent edge with payload `0` is treated as silent and emitted with flattened weight `SILENT`.
- A parent edge with payload `SILENT` is **not** treated as silent.
- The same edge then falls through to the child-spawn branch, gets rounded to a huge child index, fails `child_index >= k`, and is silently dropped from the flattened automaton.

This is inconsistent with the documented model and with accepted input syntax:

- `README.md:173` states that parent silent transitions use the `SILENT` keyword.
- `src/Parser.cpp:475-478` maps unreadable weights such as `SILENT` to the distinguished `SILENT` value.
- `samples/nested/nested_sil1.txt:2` contains an actual parent `SILENT` transition.

The current nested emptiness tests do not appear to exercise this case directly.

Concrete runtime evidence:

- Command:
  - `./quak-nested samples/nested/nested_sil1.txt non-empty LimSupAvg Max_f 1`
- Observed result:
  - `isNonEmpty(LimSupAvg, Max_f, threshold=1) = 0`
- Why this looks wrong:
  - in `samples/nested/nested_sil1.txt`, the word `b(ba)^\omega` yields one child-1 return `0` followed by infinitely many child-2 returns `1`
  - under the documented semantics where parent `SILENT` steps emit no value, the resulting return sequence is `0,1,1,1,...`, so `LimSupAvg >= 1` should hold
- Differential sanity check:
  - `./quak-nested samples/nested/nested_sil1.txt non-empty LimSup Max_f 1` returns `1`
  - `./quak-nested samples/nested/nested_sil1.txt non-empty Inf Max_f 1` returns `0`
  - this pattern is consistent with the intended sample semantics and makes the `LimSupAvg + Max_f` failure point more specifically at the `flatten_regular` family

This runtime check goes through a `flatten_regular`-backed path and makes the issue user-visible, not just a static mismatch.

Relevant code:

- `src/NestedAutomaton.cpp:1674-1705`
- `src/Parser.cpp:475-478`
- `README.md:173`
- `samples/nested/nested_sil1.txt:1-5`

#### 5. Efficiency issue: parent good-mask is recomputed once per child in `flatten_regular`

- Location: `src/NestedAutomaton.cpp:303`, `src/NestedAutomaton.cpp:407`, `src/NestedAutomaton.cpp:1571`
- Severity: low
- Status: open

`flatten_regular` computes child return values for every child in a loop. Each call to `computeChildReturnValuesParentAware` rebuilds the same parent-good mask by calling `computeParentGoodMask(nwa)`.

That work depends only on the parent automaton, not on the specific child, so it is redundant across the whole loop.

This is not a correctness bug, but it adds avoidable graph work to every `flatten_regular` invocation and is especially wasteful on instances with many children.

Relevant code:

- `src/NestedAutomaton.cpp:195-289` (`computeParentGoodMask`)
- `src/NestedAutomaton.cpp:291-390` (`computeMinMaxReturnValuesParentAware`)
- `src/NestedAutomaton.cpp:393-516` (`computeSumBReturnValuesParentAware`)
- `src/NestedAutomaton.cpp:1563-1572` (`flatten_regular` loop over children)

#### 6. Summary note: nested backends use incompatible encodings for parent silence

- Location: multiple sites in `src/NestedAutomaton.cpp`
- Severity: low
- Status: open

The nested implementation currently mixes at least three different assumptions for “silent / no child” parent transitions:

- parser/documented syntax: `SILENT` is stored as the distinguished floating value `weight_t(SILENT)`
- some backends: `0` means silent because it denotes dummy child `0`
- average-case pipeline comments/code: negative payload means silent

This is primarily a roll-up note tying together the concrete `SILENT`-handling bugs already recorded elsewhere. Several backends decode the raw parent payload directly as a child index and then dereference `getChild(child_id)` or branch on child size, which is unsafe for a true parser-level `SILENT` value.

Concrete examples:

- `flatten_regular` treats only `pw == 0` as silent and drops actual `SILENT` edges instead (`src/NestedAutomaton.cpp:1677-1705`).
- `flatten_SumPlusMinus_Sup` decodes `child_id` directly from `to_float()` and immediately calls `getChild(child_id)->getStates()->size()` (`src/NestedAutomaton.cpp:3766-3769`).
- `flatten_SumPlusMinus_Inf` does the same via `to_uint()` (`src/NestedAutomaton.cpp:4304-4309`).
- `flatten_MinMax_Sup` also decodes directly from `to_float()` (`src/NestedAutomaton.cpp:4828-4831`).
- `synchronizeChildren` / `flatten_Avg_SumMinus` assume payload `<= 0` means silent (`src/NestedAutomaton.cpp:2255-2263`, `src/NestedAutomaton.cpp:2463-2468`, `src/NestedAutomaton.cpp:2531-2533`, `src/NestedAutomaton.cpp:2775-2788`), which again does not match parser-level `SILENT`.

Impact:

- true `SILENT` inputs accepted by the parser and described in the README are not handled consistently by the nested decision procedures
- depending on the backend, such transitions may be dropped, misclassified, or used as out-of-range child indices

Important nuance:

- many generated benchmark families labeled “silent” actually encode those steps as child index `0`, which masks this issue on those inputs
- the bug still affects the documented input model and examples such as `samples/nested/nested_sil1.txt`

#### 7. Efficiency issue: `flatten_regular` carries dead state information via `last_guess`

- Location: `src/NestedAutomaton.cpp:1485-1513`, `src/NestedAutomaton.cpp:1630`, `src/NestedAutomaton.cpp:1688`, `src/NestedAutomaton.cpp:1736`
- Severity: low
- Status: open

`BuchiState_obl` includes `last_guess`, and that field participates in equality and ordering, so it splits the constructed state space. However, within `flatten_regular`, `last_guess` is not consulted by:

- `advance_phase`
- the transition relation
- the acceptance condition

The field is written into successor states but never read for semantics. That means otherwise identical states are kept distinct solely because they were reached through different last emitted guesses.

Impact:

- unnecessary state duplication in `state_map`
- larger worklist and more edges in the flattened automaton

This appears to be a pure performance cost rather than a correctness issue.

### 2026-04-16: Completion and `SumMinus + LimAvg` preprocessing

#### 8. Correctness bug: `makeCompleteNested` underallocates parent weights when a new sink weight is needed

- Location: `src/NestedAutomaton.cpp:1798-1815`, `src/Map.h:84-86`
- Severity: medium
- Status: open

In `makeCompleteNested`, the parent sink-weight lookup uses an unsigned sentinel:

- `unsigned int parent_sink_weight_id = -1;`
- `size_t num_weights = this->getWeights()->size() + ((parent_sink_weight_id > -1) ? 1 : 0);`

Because `parent_sink_weight_id` is unsigned, the test `parent_sink_weight_id > -1` is effectively always false. As a result, `num_weights` is never increased for a new parent sink weight.

The function then does:

- `new_weights->insert(this->getWeights()->size(), parent_sink_weight);`

If the sink weight is not already present, that insert is out of bounds:

- in debug-style builds, `MapArray::insert` asserts on `key < capacity`
- in non-asserting builds, this becomes an unchecked out-of-bounds write

Important scope note:

- the default `parent_sink_w = 0` masks this on inputs whose parent weight table already contains `0`
- the bug is still real for valid inputs without parent weight `0`, and for explicit custom sink weights such as the existing `makeCompleteNested(..., -1, -1)` test shape

Relevant code:

- `src/NestedAutomaton.cpp:1778-1985` (`makeCompleteNested`)
- `src/Map.h:64-94` (`MapArray`)
- `src/NestedAutomaton.h:57-59` (default sink weights)

#### 9. Robustness bug: `compute_c_bound` aborts on small supported inputs due to bound explosion

- Location: `src/NestedAutomaton.cpp:2710-2769`
- Severity: medium
- Status: open

`compute_c_bound` builds a very large theoretical upper bound:

- `conf_upper = |Q_m| * 2^{|Q_slv|}`
- `N = (|Q_slv| + 2) * conf_upper * |Q_slv|^{2|Q_slv|}`
- `c_bound = 2 * N`

The implementation aborts on overflow via `QUAK_FAIL` instead of degrading gracefully or reporting the instance as unsupported.

Concrete runtime evidence:

- Command:
  - `./quak-nested samples/nested/compute_return_max.txt non-empty LimSupAvg SumMinus 0`
- Observed result:
  - `Failure: Overflow in sat_mul_u64 during compute_c_bound`

This is a small valid nested automaton:

- parent has 2 states
- there are 4 real children
- total child-state sum is only 12

#### 10. Correctness bug: real one-state children are misclassified as dummy/silent

- Location: multiple sites in `src/NestedAutomaton.cpp`
- Severity: medium
- Status: open

The implementation repeatedly uses `child->getStates()->size() <= 1` or `< 2` as a proxy for “dummy child / silent step”. That assumption is not part of the documented nested-automaton model:

- `README.md:159-169` and `assumptions.md:62-74` reserve **child index 0** for the dummy child
- they do **not** require every non-dummy child to have at least two states
- the parser only enforces that non-dummy children have a `final:` declaration (`assumptions.md:53`, `README.md:165`)

Affected sites include:

- `computeMinMaxReturnValuesParentAware`: returns no values for any child of size `<= 1` (`src/NestedAutomaton.cpp:295-296`)
- `computeSumBReturnValuesParentAware`: same (`src/NestedAutomaton.cpp:397-398`)
- `flatten_regular`: skips child table construction for children with `< 2` states (`src/NestedAutomaton.cpp:1601-1602`)
- `determinizeWithMacroAlphabet`: skips edges from children with `< 2` states (`src/NestedAutomaton.cpp:2143-2146`)
- `synchronizeChildren`: refuses to pick a canonical alphabet unless some child has `> 1` state (`src/NestedAutomaton.cpp:2292-2297`)
- monotone backends also decode `size() == 1` as silent in their parent-step handlers (`src/NestedAutomaton.cpp:3768`, `4309`, `4830`, `5446`, `6154`, `6605`)

Concrete runtime evidence:

- Witness file:
  - `/tmp/one_state_real_child.txt`
  - contents:
    ```text
    @PARENT
    a : 1, q0 -> q0

    @CHILD 0

    @CHILD 1
    final: s
    a : 1, s -> s
    ```
- Intended semantics:
  - every parent step invokes real child `1`
  - the child consumes that same `a` immediately, takes `a : 1, s -> s`, and is in a final state
  - the induced return-value sequence is `1,1,1,...`
- Observed results:
  - `./quak-nested /tmp/one_state_real_child.txt non-empty Sup Max_f 1` -> `0`
  - `./quak-nested /tmp/one_state_real_child.txt non-empty LimSup Max_f 1` -> `0`
  - `./quak-nested /tmp/one_state_real_child.txt non-empty LimSupAvg Max_f 1` -> `0`

Those should all be true under the documented semantics. This is a separate issue from parser-level `SILENT`: here the payload is an ordinary child index (`1`), but the backends erase the call because the child happens to have one state.

The same structural assumption also breaks the average-case `SumMinus` pipeline:

- Witness file:
  - `/tmp/one_state_real_child_summinus.txt`
  - contents:
    ```text
    @PARENT
    a : 1, q0 -> q0

    @CHILD 0

    @CHILD 1
    final: s
    a : -1, s -> s
    ```
- Observed result:
  - `./quak-nested /tmp/one_state_real_child_summinus.txt non-empty LimSupAvg SumMinus -1`
  - fails with `Failure: synchronizeChildren: no child alphabet available`

So one-state real children are currently outside the effective implementation domain even though the input language and semantics permit them.

The same structural assumption also breaks the degenerate but valid “no real child ever exists” case:

- witness file:
  - `/tmp/sumplus_trivial_accept_bug.txt`
- observed results:
  - `./quak-nested /tmp/sumplus_trivial_accept_bug.txt non-empty LimSupAvg SumMinus 0`
  - `./quak-nested /tmp/sumplus_trivial_accept_bug.txt non-empty LimInfAvg SumMinus 0`
  - both abort with `Failure: synchronizeChildren: no child alphabet available`

Those instances should simply be decided as empty, not rejected as unsupported by the implementation.

#### 11. Correctness risk: `flatten_Avg_SumMinus` stores multiplicities in `uint32_t` despite a `uint64_t` cap

- Location: `src/NestedAutomaton.cpp:2776-3056`
- Severity: medium
- Status: open

`flatten_Avg_SumMinus` receives a multiplicity cap `c_bound` as `uint64_t`, and `step_multiset` correctly tracks bucket counts as `uint64_t` while enforcing `ref <= c_bound` (`src/NestedAutomaton.cpp:3016-3017`).

However, the flattened-state key stores the sparse multiset as:

- `std::vector<uint32_t> nz` (`src/NestedAutomaton.cpp:2906`)
- counts are written back with `static_cast<uint32_t>(p.second)` (`src/NestedAutomaton.cpp:3034`)

This silently truncates any legal count above `UINT32_MAX`, so the state encoding cannot represent the full search space that the same function claims to allow.

This is not a purely theoretical edge:

- `compute_c_bound` itself returns `uint64_t`
- for `|Q_m| = 1` and total real-child size `|Q_slv| = 5`, the documented formula gives
  - `conf_upper = 1 * 2^5 = 32`
  - `N = (5 + 2) * 32 * 5^{10} = 2,187,500,000`
  - `c_bound = 2 * N = 4,375,000,000`
- `4,375,000,000 > UINT32_MAX`

So even on a small supported instance with five total child states, the bound passed into `flatten_Avg_SumMinus` can already exceed the representable range of its `Key::nz` counts.

This should be treated as a correctness bug, not just a performance issue: two different legal multisets can collapse to the same truncated flattened state.

### 2026-04-16: Remaining monotone backend review

#### 12. Correctness bug: several monotone backends mishandle children whose initial state is already final

- Location: multiple monotone backend handlers in `src/NestedAutomaton.cpp`
- Severity: medium
- Status: open

Several monotone flattenings spawn a called child by marking its **initial state** as active and then immediately run the global child-step logic on the current symbol. That is the right high-level structure, but some of those handlers first test whether the current child state is final and, if so, terminate the child **before** consuming the current symbol.

That contradicts the project semantics recorded in the repo: a summoned child starts consuming the current input letter immediately. Being in a final initial state should not let it terminate without reading that call-site symbol.

Concrete code patterns:

- `flatten_SumPlusMinus_Sup`:
  - `explore_global_child_transition_supremum` checks `if (child_state->getFinal())` before taking any `data->symbol` transition (`src/NestedAutomaton.cpp:3660-3673`)
- `flatten_MinMax_Sup`:
  - `explore_global_child_transition_min_max_supremum` does the same (`src/NestedAutomaton.cpp:4747-4755`)
- active `flatten_MinMax_Inf`:
  - after spawning, the per-symbol move collection skips active children already in final states with `if (child_state->getFinal()) continue;` (`src/NestedAutomaton.cpp:6667-6668`)
  - so a newly spawned child whose initial state is final is treated as already terminated, not as a child that must still consume the current symbol

Concrete runtime evidence:

- Witness file:
  - `/tmp/initial_final_child_two_states.txt`
  - contents:
    ```text
    @PARENT
    a : 1, q0 -> q0

    @CHILD 0

    @CHILD 1
    final: s t
    a : 1, s -> t
    a : 1, t -> t
    ```
- Intended semantics:
  - every call starts in final state `s`, but the child must still consume the current `a`
  - it takes `a : 1, s -> t` and ends in final state `t`
  - the induced return-value sequence is again `1,1,1,...`

Observed results on the same witness:

- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty LimSupAvg Max_f 1` -> `1`
- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty Sup Max_f 1` -> `0`
- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty LimSup Max_f 1` -> `0`
- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty Inf Max_f 1` -> `0`
- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty LimInf Max_f 1` -> `0`

This differential is useful:

- `LimSupAvg + Max_f` goes through `flatten_regular` and accepts the witness
- the monotone `Max_f` backends reject it

So this is not a global ambiguity about the input model. It is a backend-specific semantic bug in the confirmed monotone encodings.

I also checked the analogous `SumPlus` `Sup/LimSup` paths on the same witness:

- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty Sup SumPlus 1` -> `0`
- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty LimSup SumPlus 1` -> `0`

That strongly suggests the same spawn-time final-state bug affects at least the supremum-side `SumPlus` backend as well.

Control check:

- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty Sup Min_f 1` -> `1`
- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty LimSup Min_f 1` -> `1`
- `./quak-nested /tmp/initial_final_child_two_states.txt non-empty LimSupAvg Min_f 1` -> `1`

So the confirmed scope of this bug is currently:

- `Sup/LimSup + Max_f`
- active `Inf/LimInf + Max_f`
- at least `Sup/LimSup + SumPlus`

#### 13. Retraction: no supported-input counterexample currently confirmed for non-terminating background children in `Sup/LimSup`

- Location: `src/NestedAutomaton.cpp:3506-3795`, `src/NestedAutomaton.cpp:4606-4857`
- Severity: low
- Status: needs new witness

The original missing-transition witness was invalid because supported inputs require every non-final child state to have at least one outgoing transition on every alphabet symbol.

A later attempted “complete non-terminating child” witness also turned out not to establish a bug once the repository’s additional shape assumptions were enforced:

- final states must be reachable from the initial child state
- final states must not have outgoing transitions
- the parent must not admit some other accepting word that lets the allegedly non-terminating child terminate later

After tracing the implementations carefully, the currently visible `Sup/LimSup` state machine does appear to carry the relevant background obligation:

- background children are propagated through `activation` and `tracking` bits
- when `tracking_from` is all-zero, `explore_global_finalization_*_supremum` emits a one-step accepting pulse and resets `tracking_to` to the all-ones sentinel
- on the next step, only actually active children re-materialize tracked bits, because `new_tracking[ii]` is set only when both the child is active and `old_tracking[i] == 1`
- a background child that remains forever in a non-final cycle therefore keeps a tracked bit alive and blocks future accepting pulses

The stricter control witness explored during this review now returns `0` for:

- `Sup + Max_f`
- `LimSup + Max_f`
- `Sup + SumPlus`
- `LimSup + SumPlus`

So the earlier “ignore non-terminating background child” claim is currently unsubstantiated. The `succs == nullptr` branches are still suspicious for unsupported incomplete inputs, but I do not currently have a valid supported-input false-positive witness for this item.

#### 14. Retraction: no supported-input counterexample currently confirmed for non-terminating active children in `Inf/LimInf`

- Location: `src/NestedAutomaton.cpp:4126-4289`
- Severity: low
- Status: needs new witness

This item reused the same family of invalid witnesses as finding 13. Under the stricter well-formedness assumptions above, the revised control witness no longer reproduces a wrong answer:

- `./quak-nested ... non-empty Inf SumPlus 1` -> `0`
- `./quak-nested ... non-empty LimInf SumPlus 1` -> `0`

So the earlier “active child can run forever and still be ignored” claim is also not currently backed by a supported-input counterexample.

#### 15. Correctness bug: `SumMinus + LimAvg` synchronization mishandles children whose initial state is final

- Location: `src/NestedAutomaton.cpp:2416-2444`, `src/NestedAutomaton.cpp:2477-2483`, `src/NestedAutomaton.cpp:2514-2517`
- Severity: medium
- Status: open

The `SumMinus + LimInfAvg/LimSupAvg` pipeline first builds a synchronized ultimate child in `synchronizeChildren()`. That construction seeds one `U_sync` start state per parent call site:

- `SyncKey start{mid, child_index, s0, weight_t(0), false};`

But `get_or_make_state` marks a `U_sync` state final whenever the underlying child state is final and `pending_accept == false`, and the BFS then refuses to explore outgoing transitions from such a state:

- `if (child_is_final(key.child_index, key.child_state_id) && !key.pending_accept) ns->setFinal(true);`
- `if (child_is_final(cur.child_index, cur.child_state_id) && !cur.pending_accept) continue;`

That means if a real child starts in a final state, the synchronized pipeline treats the call as already terminated **before** consuming the call-site symbol. This contradicts the repo semantics, where a child starts reading on the current parent letter immediately.

Concrete runtime evidence:

- Witness file:
  - `/tmp/initial_final_summinus_avg.txt`
  - contents:
    ```text
    @PARENT
    a : 1, q0 -> q0

    @CHILD 0

    @CHILD 1
    final: s t
    a : -1, s -> t
    a : -1, t -> t
    ```
- Intended semantics:
  - every call starts in final state `s`, but still consumes the current `a`
  - it takes `a : -1, s -> t` and returns `-1`
  - the induced return sequence is `-1,-1,-1,...`
  - so both `LimSupAvg` and `LimInfAvg` at threshold `-1` should be true

Observed results:

- `./quak-nested /tmp/initial_final_summinus_avg.txt non-empty LimSupAvg SumMinus -1` -> `0`
- `./quak-nested /tmp/initial_final_summinus_avg.txt non-empty LimInfAvg SumMinus -1` -> `0`

This is consistent with the pipeline building a parent payload that spawns into a `U_sync` state with no outgoing transitions. In `flatten_Avg_SumMinus`, any spawned multiset bucket whose `ustep[sid][a]` entry is missing causes the whole letter step to fail (`src/NestedAutomaton.cpp:2997-3004`).

#### 18. Robustness / correctness risk: `synchronizeChildren` uses `unordered_map` keys with epsilon equality but raw-float hashing

- Location: `src/Weight.h:66-82`, `src/NestedAutomaton.cpp:2389-2406`
- Severity: low
- Status: open

`weight_t` equality is epsilon-based:

- `bool operator==(weight_t rhs) const { return fabs(value - rhs.value) < EPSILON; }`

but its hash is the raw `float` hash:

- `return std::hash<float>{}(x.value);`

`synchronizeChildren` then stores `SyncKey` objects in an `std::unordered_map`, and `SyncKey::operator==` compares the accumulator via that epsilon-based `weight_t` equality:

- `accumulator == o.accumulator`

while `SyncKeyHash` hashes the accumulator with `std::hash<weight_t>{}(k.accumulator)`.

That violates the `unordered_map` contract: equal keys must hash equally. In practice this can lead to duplicate “equal” synchronization states, missed lookups, or other undefined behavior when floating-point accumulation produces values that compare equal within epsilon but hash differently.

I do not yet have a minimal runtime witness that turns this into a user-visible wrong answer, so this is recorded as a static correctness/robustness risk rather than a confirmed behavioral counterexample.

#### 19. Efficiency issue: `flatten_Avg_SumMinus` repeatedly rebuilds and resorts sparse multisets on every explored edge

- Location: `src/NestedAutomaton.cpp:2979-3035`
- Severity: low
- Status: open

Inside `flatten_Avg_SumMinus`, the local `step_multiset` helper does all of the following from scratch for every explored flattened transition:

- decodes `Key::nz` into an `unordered_map<uint32_t, uint64_t> cur`
- allocates a second `unordered_map<uint32_t, uint64_t> nxt`
- advances every active bucket
- materializes a temporary vector of `(sid, count)` pairs
- sorts that vector
- re-encodes it back into the canonical sparse `std::vector<uint32_t> nz_out`

This is structurally expensive because the BFS around it already explores a large flattened state space. The same multiset is being decoded and re-encoded once per symbol, per state, per outgoing parent edge.

This looks like a real hotspot rather than a micro-optimization:

- it sits in the innermost transition loop of the final `SumMinus + LimAvg` flattening
- it performs multiple hash-table allocations and a full sort even when the active support changes only slightly
- the pipeline already has large theoretical multiplicity bounds, so active-support sizes can be nontrivial before the decision procedure finishes

Once the correctness bugs in this pipeline are fixed, this decode/advance/sort/re-encode loop is one of the first places I would target for a serious speedup.

#### 20. Correctness bug: `determinizeWithMacroAlphabet` can copy one child’s resolver edges into a different child

- Location: `src/NestedAutomaton.cpp:2125-2165`
- Severity: high
- Status: open

In `determinizeWithMacroAlphabet`, child resolver buckets from a `MacroSymbol` are copied with two indices:

- `ai` walks resolver buckets
- `ci` walks concrete child arrays

But when a resolver bucket is empty, the code advances only `ai`:

- `if (edges.size() == 0) { ++ai; continue; }`

It does **not** advance `ci`. That is harmless for the dummy child at bucket `1`, but it is wrong for any real child whose resolver set happens to be empty for that macro. After the first such empty real-child bucket, all later non-empty buckets are copied into the wrong child automata.

This is not just a static concern; it produces user-visible false positives in the `SumMinus + LimAvg` pipeline.

Concrete runtime evidence:

- Witness file:
  - `/tmp/determinize_bucket_misalignment.txt`
  - contents:
    ```text
    @PARENT
    a : 1, q0 -> q0
    b : 0, q0 -> q0
    b : 0, q0 -> q1
    a : 0, q1 -> q1
    b : 0, q1 -> q1

    @CHILD 0

    @CHILD 1
    final: f1
    b : 0, s1 -> f1

    @CHILD 2
    final: f2
    a : -1, t0 -> f2
    ```
- Semantic reasoning:
  - the only non-silent parent symbol is `a`, and it always calls child `1`
  - child `1` has no `a` transition from its initial state, so every non-silent call is invalid
  - `b`-steps are silent, so they cannot satisfy the parent acceptance condition by themselves
  - therefore there is no valid accepting nested run at all

Observed results:

- `./quak-nested /tmp/determinize_bucket_misalignment.txt non-empty LimSupAvg SumMinus -1` -> `1`
- `./quak-nested /tmp/determinize_bucket_misalignment.txt non-empty LimInfAvg SumMinus -1` -> `1`

The most plausible explanation is exactly the bucket-shift bug above:

- for symbol `a`, child `1` contributes an empty resolver bucket
- child `2` contributes a non-empty resolver bucket
- the copy loop skips the empty bucket without advancing `ci`
- child `2`’s `a`-edge is copied into child `1` in the determinized automaton

That makes the downstream synchronized pipeline believe the parent’s calls to child `1` can return `-1`, even though no such run exists in the original nested automaton.

#### 21. Efficiency issue: `Sup/LimSup` monotone backends enumerate full successor products of active children

- Location: `src/NestedAutomaton.cpp:3578-3635`, `src/NestedAutomaton.cpp:4680-4727`
- Severity: low
- Status: open

Both supremum-side monotone constructions propagate background children by recursive enumeration over every active child-state and every one of its outgoing edges on the current symbol:

- `explore_global_selection_supremum` for `SumPlus/SumMinus + Sup/LimSup`
- `explore_global_selection_min_max_supremum` for `Min_f/Max_f + Sup/LimSup`

This is a full Cartesian-product exploration of successor choices across concurrently active children. Even when the distinguished tracked token is fixed, the background part still branches once per outgoing edge of every active child-state before a single flattened edge is emitted.

That matches the current benchmark pain points:

- `results/paper/response_sup_sumplus_emptiness.csv:2` reports about `338.98s` for `Sup + SumPlus`
- the repo’s other expensive nested emptiness paths are likewise exactly the constructions with the largest concurrent-child search spaces

This is not a correctness bug, but it is one of the clearest algorithmic reasons the supremum-side monotone backends blow up. Any serious performance work on those paths will probably need a more symbolic/background-aggregate treatment instead of raw successor-product enumeration.

So the current `SumMinus + LimAvg` emptiness path can abort on modest inputs before flattening even begins.

This is not just a “large benchmark” limitation. It is a user-visible failure mode on ordinary repository samples.

Relevant code:

- `src/NestedAutomaton.cpp:2710-2769` (`compute_c_bound`)
- `src/NestedAutomaton.cpp:6991-7010` (`isNonEmpty` calling path)
- `samples/nested/compute_return_max.txt:1-37`

## Additional Coverage Notes

- `src/tests/sanity_tests/test_flatten_avg_summinus.cpp` does not exercise the production `compute_c_bound` formula. Its helper uses `max_child_states * parent_states` instead (`src/tests/sanity_tests/test_flatten_avg_summinus.cpp:11-23`).
- Because of that substitution, the current tests do not cover the overflow behavior in finding 9 and do not validate that the production bound computation matches the assumptions of `flatten_Avg_SumMinus`.

### 2026-04-16: Monotone `Min_f/Max_f` backends

#### 10. Correctness bug: active `flatten_MinMax_Inf` is unsound for overlapping `Max_f` calls

- Location: `src/NestedAutomaton.cpp:6510-6515`, `src/NestedAutomaton.cpp:6728-6752`
- Severity: high
- Status: open

The active simplified `flatten_MinMax_Inf` stores, for `Max_f`, only one status bit-pattern per flattened child state:

- `0` = inactive
- `1` = active and has not yet seen a high edge
- `3` = active and has seen a high edge

This loses information when two simultaneously active invocations of the **same** child reach the **same** child state with different histories:

- one token has already seen an edge `>= threshold`
- another token has not

The implementation merges them by OR-ing into a single `result_status[to_idx]`, so the merged state keeps only the stronger “seen high” summary and forgets the weaker token entirely.

Why this is unsound:

- for `Inf/LimInf + Max_f`, **every** invocation must eventually terminate with a max value at least `threshold`
- if a low-history token and a high-history token merge, the low-history obligation is still real
- the simplified construction can later terminate the merged token successfully even though one original invocation never saw a high edge

Concrete counterexample sketch:

- parent forces the periodic pattern `a b c a b c ...`
- on `a`, invoke child 1 and move parent to the next control state
- on `b`, invoke child 1 again
- on `c`, take a silent/dummy-child step back to the start
- child 1 behaves as follows:
  - invocation on `a` reaches state `s1` with only low edges seen
  - invocation on `b` reaches the same state `s1` but with a high edge already seen
  - both then take `c` to final through a low edge

True semantics:

- the `a`-spawned invocation returns `0`
- the `b`-spawned invocation returns `2`
- the infinite return sequence is `0, 2, 0, 2, ...`
- therefore `Inf >= 1` and `LimInf >= 1` should both be false

Observed runtime evidence from the current binary:

- `./quak-nested /tmp/max_merge_bug.txt non-empty Inf Max_f 1`
  - returns `1`
- `./quak-nested /tmp/max_merge_bug.txt non-empty LimInf Max_f 1`
  - returns `1`

The temporary witness used for this check was:

```text
@PARENT
a : 1, q0 -> q1
b : 1, q1 -> q2
c : 0, q2 -> q0

@CHILD 1
final: f
a : 0, s0 -> s1
b : 2, s0 -> s1
b : 0, s1 -> s1
c : 0, s1 -> f
```

This bug is specific to the **active simplified** implementation. The archived `flatten_MinMax_Inf_v2` keeps separate `1_0` / `1_1` status classes and does not collapse this distinction.

Relevant code:

- `src/NestedAutomaton.cpp:6510-6515` (simplification rationale)
- `src/NestedAutomaton.cpp:6523-6526` (`child_status` representation)
- `src/NestedAutomaton.cpp:6728-6752` (status merge at shared destination)
- `src/NestedAutomaton.cpp:6381-6504` (archived `v2` for contrast)

#### 11. Efficiency issue: active `flatten_MinMax_Inf` enumerates the full Cartesian product of active child moves

- Location: `src/NestedAutomaton.cpp:6642-6713`
- Severity: low
- Status: open

The active `flatten_MinMax_Inf` first collects all possible moves for every active child-state and then explicitly materializes the full Cartesian product of those move sets before processing combinations.

That gives worst-case branching proportional to:

- `Π_i outdeg(active_child_i)`

and it does so by storing the full `combinations` vector in memory.

Consequences:

- exponential blow-up in the number of concurrently active children
- unnecessary memory pressure from materializing all combinations up front instead of streaming/backtracking through them
- this is especially dangerous in the same backend that already tracks all active child states explicitly

This is an efficiency issue even if the semantics were otherwise correct.

## Coverage Notes For Monotone Backends

- The repository has many broad `Max_f` correctness tests, but I have not found a case that specifically exercises overlapping invocations of the same child converging to the same state with different `Max_f` histories.
- The existing `flatten_MinMax_Inf` sanity tests mostly check that a flattened automaton is produced and that coarse statistics vary with thresholds; they do not pin down the merge case from finding 10.

### 2026-04-16: Monotone `SumPlus/SumMinus` infimum backend

#### 12. Correctness bug: `flatten_SumPlusMinus_Inf` loses overlapping calls at the same child state

- Location: `src/NestedAutomaton.cpp:4333-4354`
- Severity: high
- Status: open

The active `flatten_SumPlusMinus_Inf` stores at most **one** `BudgetSet` per flattened child state. When a new parent invocation would spawn another active token into a state that already carries a budget, the code explicitly gives up on representing both:

- comment in code: `// Token exists -- can't represent two tokens at same state, keep existing tracking`

That is not semantics-preserving for `Inf/LimInf`, because two overlapping invocations at the same child state can have different remaining requirements and both obligations still matter.

Concrete false-negative witness for `SumPlus + Inf/LimInf`:

```text
@PARENT
b : 1, q0 -> q1
a : 1, q1 -> q2
c : 0, q2 -> q0

@CHILD 1
final: f
b : 2, s0 -> s1
a : 0, s0 -> s1
a : 0, s1 -> s1
c : 1, s1 -> f
```

Forced parent word: `(bac)^\omega`

Child-return sequence under the intended semantics:

- token spawned on `b`: value `2 + 0 + 1 = 3`
- token spawned on `a`: value `0 + 1 = 1`
- repeated forever, so the sequence is `3,1,3,1,...`

Therefore:

- `Inf >= 1` should be true
- `LimInf >= 1` should be true

Observed runtime evidence from the current binary:

- `./quak-nested /tmp/sumplus_collision_false_negative.txt non-empty Inf SumPlus 1`
  - returns `0`
- `./quak-nested /tmp/sumplus_collision_false_negative.txt non-empty LimInf SumPlus 1`
  - returns `0`
- control check on the same sample:
  - `./quak-nested /tmp/sumplus_collision_false_negative.txt non-empty Sup SumPlus 1` returns `1`
  - `./quak-nested /tmp/sumplus_collision_false_negative.txt non-empty LimSup SumPlus 1` returns `1`

Why this happens:

- after the `b` step, the first active token reaches `s1`
- on the following `a` step, a second token is spawned and also reaches `s1`
- the implementation keeps only the old budget at `s1` instead of representing both active calls
- that can force `global_edge_weight = 0` on a step where all real invocations are still satisfiable, yielding a false negative for `Inf/LimInf`

This was confirmed on `SumPlus`. The same representational limitation likely threatens `SumMinus + Inf/LimInf` as well, since it uses the same one-budget-per-state encoding and the same collision branch.

Relevant code:

- `src/NestedAutomaton.cpp:4298-4363` (`explore_global_parent_transition`)
- especially `src/NestedAutomaton.cpp:4333-4354`

## Additional Coverage Notes

- The dedicated `flatten_SumPlusMinus_Inf` sanity tests focus on construction viability, output shape, and threshold variation; they do not contain an overlapping-call witness like finding 12.
- The broad correctness suite appears to miss this pattern as well, despite extensive coverage over many automata/aggregator combinations.

### 2026-04-16: Support-matrix / interface consistency

#### 13. Interface mismatch: CLI rejects `SumMinus + Sup/LimSup/Inf/LimInf` although the dispatcher and README expose it

- Location: `src/quak-nested-main.cpp:277-281`, `README.md:333-339`, `CLI.md:62-66`
- Severity: low
- Status: open

There is a three-way inconsistency:

- `NestedAutomaton::isNonEmpty` dispatches `SumMinus + (Sup | LimSup | Inf | LimInf)` to the monotone 0/1 backend (`src/NestedAutomaton.cpp:6977-6985`)
- `README.md` documents the same support matrix (`README.md:335`)
- but the CLI hard-rejects every non-`LimAvg` `SumMinus` non-emptiness query

Concrete runtime evidence:

- `./quak-nested /tmp/summinus_collision_false_negative.txt non-empty Inf SumMinus -1`
  - returns the error:
    - `SumMinus finite aggregator only supports LimInfAvg or LimSupAvg.`

Impact:

- the monotone `SumMinus` backend exists in the library layer but is not reachable from the main nested CLI
- this also makes it harder to validate or benchmark those code paths end to end

Relevant code:

- `src/quak-nested-main.cpp:277-281`
- `src/NestedAutomaton.cpp:6977-6985`
- `README.md:333-339`
- `CLI.md:62-66`

#### 14. Interface mismatch: CLI rejects nested universality for `SumPlus` and `SumMinus` although the library and README expose it

- Location: `src/quak-nested-main.cpp:266-269`, `src/NestedAutomaton.cpp:7076-7116`, `README.md:120-125`, `README.md:344`
- Severity: low
- Status: open

The public surfaces disagree again for universality:

- the library implementation of `NestedAutomaton::isUniversal` accepts `SumPlus` and `SumMinus` for `Inf/Sup/LimInf/LimSup`, and internally reduces them to `SumB`
- the README support table also lists `SumPlus` and `SumMinus` as supported for universality
- but the CLI rejects both with:
  - `Nested universal does not support SumPlus or SumMinus.`

Impact:

- the documented library API and the main executable expose different supported-combination sets
- users relying on the CLI cannot exercise a code path that the library and docs describe as available

Relevant code:

- `src/quak-nested-main.cpp:266-269`
- `src/NestedAutomaton.cpp:7076-7116`
- `README.md:120-125`
- `README.md:344`

## Cross-Cutting Performance Priorities

Current review evidence points to these priority hotspots:

1. `SumPlus + LimSupAvg`
   - Existing benchmark artifact already times out:
     - `results/paper/response_limsupavg_sumplus_emptiness.csv:2`
   - Code-level drivers already identified:
     - duplicated fast-path/slow-path work in `isNonEmpty` (finding 3)
     - fallback into `flatten_regular`, which still carries the `last_guess` state blow-up (finding 7)

2. `SumPlus + Sup`
   - Existing benchmark artifact is extremely slow even when it finishes:
     - `results/paper/response_sup_sumplus_emptiness.csv:2` reports about `338.98s`
   - Likely drivers:
     - large witness/background bookkeeping in `flatten_SumPlusMinus_Sup`
     - state-space growth from activation/tracking vectors over flattened child-state space

3. `Max_f + LimSupAvg`
   - Existing benchmark artifact is also very expensive on small inputs:
     - `results/paper/resource_limsupavg_max_emptiness.csv:2-3` reports about `110.47s` and `76.04s`
   - Likely drivers:
     - `flatten_regular` state blow-up
     - per-child recomputation of parent-good masks (finding 5)

4. `SumMinus + LimAvg`
   - This path has both correctness and robustness blockers before pure optimization:
     - wrong `c_bound` source (finding 1)
     - parent sink-weight underallocation in completion (finding 8)
     - hard overflow aborts in `compute_c_bound` on small valid inputs (finding 9)
   - Even after those are fixed, `synchronizeChildren` and `flatten_Avg_SumMinus` remain intrinsically high-risk for blow-up.

5. `Max_f/Min_f + Inf/LimInf`
   - The active simplified backend has a concrete correctness bug (finding 10) and an explicit Cartesian-product enumeration cost (finding 11), so optimization should wait until semantics are repaired.

## Missing-Test Categories

The current suite is broad, but these gaps now matter:

- Real parser-level `SILENT` parent transitions going through each backend family, not just dummy-child `0` encodings.
- Overlapping invocations that converge to the same child state with different histories:
  - `Max_f + Inf/LimInf`
  - `SumPlus + Inf/LimInf`
- Production `compute_c_bound` behavior, including overflow handling and consistency with `flatten_Avg_SumMinus`.
- `makeCompleteNested` with genuinely new sink weights on an input whose parent weight table does not already contain that value.
- End-to-end CLI/library consistency tests for supported `(infVal, finVal)` combinations.

## Current Status Snapshot

At this point in the review, the strongest conclusions are:

- `flatten_regular` is semantically suspect on true `SILENT` inputs and carries avoidable state blow-up.
- the active `flatten_MinMax_Inf` implementation is incorrect for overlapping `Max_f` calls.
- `flatten_SumPlusMinus_Inf` is incorrect for overlapping `SumPlus` calls.
- the `SumMinus + LimAvg` pipeline has multiple independent robustness/correctness problems before optimization should even begin.
