I went through the benchmark family, the three compared entry points, and the current backend code. My conclusion is:

The right thing to ship is **not** `regular`, and **not** the old `split_witness`. It is a **cached Min/Max threshold backend** that keeps the current `threshold_extremal` semantics, but replaces its inline obligation bags with interned IDs plus memoized spawn/step operations.

Why that is the right target:

The benchmark family is intentionally brutal for explicit flattening: the parent is a one-state final loop, each real child is a deterministic resource tracker with `2^k+3` states, and the alphabet has size `n(k+2)` with many process-local no-op letters. Also, for the checked benchmark, the threshold is always `k`, so every solved instance is positive; the experiment is measuring how expensive the certification path is, not whether the instance is hard to classify.  

On that family, `threshold_extremal` is already clearly best. The comparison file says the three backends are `threshold_extremal` via `flatten_MinMax_Sup`, `regular` via `flatten_regular`, and `split_witness` via `flatten_MinMax_Sup_split_witness`.  In the benchmark note, at `(n=3,k=3)` it uses `25,300` states / `263,520` transitions / `4.28s`, while `regular` needs `73,743` / `652,236` / `14.28s` and `split_witness` needs `64,500` / `986,757` / `23.81s`. At `(n=4,k=2)`, `threshold_extremal` is `51,616` / `519,188` / `9.77s`, versus `131,981` / `1,171,000` / `27.19s` for `regular` and `199,236` / `3,258,044` / `90.06s` for `split_witness`. 

Why the other two lose:

`regular` is doing more semantic work than this benchmark needs. In its parent step, it iterates over the full set of guessed child return values, `child_return_values[child_index]`, not just the threshold question.  On this family, each child return lies in `{0,…,k}` and the query is only “is it at least `k`?”, so most of that precision is wasted. 

`split_witness` loses for the opposite reason: the semantic idea is fine, but the old implementation pays too much for explicit recursive propagation of background states. The benchmark note calls out its dominant cost directly as “explicit recursive enumeration of many active background states” plus “very large transition fanout.” 

Now the important part: **the current best still leaves a lot on the table.**

`flatten_MinMax_Sup` currently just routes to the generic `flatten_threshold_extremal_impl`.  Inside that implementation, for every explored flattened state and every symbol, it re-steps `P1` and `P2` with `thrext_step_obl_bag`, then for every real parent edge and for both guesses `0/1`, it recomputes `thrext_spawn_obligation`, and it stores the full `ThrExtBuchiState` — including the full `P1`/`P2` bags — directly in `state_map`.   That means the code is repeatedly paying for the same three subproblems:

1. stepping the same obligation on the same symbol,
2. stepping the same bag on the same symbol,
3. spawning the same `(child, guess, symbol)` obligation.

It also pays heavily for state-key comparisons because the key is a whole nested vector structure rather than compact IDs.

There is also an unused but very relevant substrate already in the codebase: a Min/Max-specific threshold representation with `MMThrFrontier` split into `y0`/`y1` bitsets, a goal-aware liveness table `MMThrLive`, and Min/Max-specific spawn/step routines. That machinery exists in `NestedAutomaton.cpp`, but `flatten_MinMax_Sup` is not using it.  

Separately, the extracted cached backend already shows the missing engineering pattern: precomputed `spawn_code`, memoized `step_obl`, memoized `step_bag`, `bag_add_obl` caching, and a compact `Key{parent,P1,P2,phase,epoch_nonempty}` stored in an `unordered_map`.   

So the implementation I recommend is:

**Cached MMThr backend for `flatten_MinMax_Sup`**

* keep the current Min/Max threshold semantics exactly,
* reuse the existing `MMThrFrontier` / `MMThrLive` representation,
* intern each obligation `(child, guess, frontier)` as an `OblId`,
* intern each sorted obligation bag as a `BagId`,
* precompute all spawn results `(child, guess, symbol)`,
* memoize singleton-obligation stepping `(OblId, symbol)`,
* memoize bag stepping `(BagId, symbol)`,
* use compact flattened states `(parent, P1_id, P2_id, phase, epoch_nonempty)`.

That is a **correct** replacement, because it does not alter the phase machine, acceptance rule, or meaning of obligations; it only canonicalizes and memoizes them. In other words, it is an isomorphic re-encoding of the current threshold backend, not a semantic redesign.

I wrote that implementation as a drop-in fragment here:

[minmax_sup_cached_mmthr_fragment.cpp](../src/archived/fragments/minmax_sup_cached_mmthr_fragment.cpp)

and a short integration note here:

[analysis_minmax_sup_scalable_backend.md](./analysis_minmax_sup_scalable_backend.md)

There is also a one-line wrapper replacement here:

[minmax_sup_wrapper_replacement.txt](../src/archived/fragments/minmax_sup_wrapper_replacement.txt)

The one thing I would **not** do first is revive old `split_witness` directly. The benchmark note already explains why that version lost. If you want a second-phase improvement after the cached threshold backend lands, the next worthwhile step is a **cached split-witness v2** that keeps one explicit witness and stores background obligations as termination-only bags. That could reduce logical state count further, but it is a riskier rewrite. The cached threshold backend above is the low-risk win.
