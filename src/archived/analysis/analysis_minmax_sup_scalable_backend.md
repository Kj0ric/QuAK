# Sup × Max_f on the resource benchmark: implementation verdict

## 1. What the results say

For the checked-in benchmark family, `threshold_extremal` is the clear winner.
Representative solved points from the benchmark note:

- `(n=3,k=3)`: `threshold_extremal` = `25,300` states / `263,520` transitions / `4.28s`,
  `regular` = `73,743` / `652,236` / `14.28s`,
  `split_witness` = `64,500` / `986,757` / `23.81s`.
- `(n=4,k=2)`: `threshold_extremal` = `51,616` states / `519,188` transitions / `9.77s`,
  `regular` = `131,981` / `1,171,000` / `27.19s`,
  `split_witness` = `199,236` / `3,258,044` / `90.06s`.

The family is hard because the parent is trivial, each child has `2^k+3` states,
there are `n` process-specific children, and the alphabet is `n(k+2)` with many
process-local no-op letters. So the bottleneck is flattening cost, not the actual
nonemptiness question.

## 2. Why each implementation behaves the way it does

### `regular`
The regular backend solves a stronger problem than needed here: it tracks full
finite return values instead of the threshold question. That extra precision is
wasted on `Sup × Max_f` at threshold `k`, where only “below threshold / at least
threshold” matters.

### `split_witness`
The witness/background idea is semantically attractive, but the old-v2 code pays
heavily for explicit recursive propagation of all active background states on every
symbol. On this family, that means large transition fanout and too many explicit
branches.

### `threshold_extremal`
This is the right semantic abstraction for the benchmark: it reduces each child to
a threshold obligation. However, the current implementation still leaves a lot of
performance on the table:

- obligations are stored inline in state keys;
- the same obligation is re-stepped many times on the same symbol;
- the same bag of obligations is re-stepped many times on the same symbol;
- spawn results for `(child,guess,symbol)` are recomputed instead of memoized;
- state lookup uses heavyweight composite keys instead of compact interned IDs.

## 3. The replacement I recommend

Use an **exact cached threshold backend** for `flatten_MinMax_Sup`, with the same
phase machine / acceptance rule as the current `threshold_extremal`, but with:

- interned obligation IDs;
- interned bag IDs;
- cached singleton-obligation stepping;
- cached bag stepping;
- precomputed spawn table;
- `unordered_map` state space over compact keys;
- Min/Max-specific boolean-threshold frontier representation (`y0`/`y1`) instead
  of the generic `ThrExtConf` vector form.

That is what the attached fragment implements.

## 4. Why this is the safest upgrade

It is **semantics-preserving** relative to the current threshold-extremal Min/Max
construction. The cache changes representation only; it does not change:

- the obligation meaning,
- the phase machine,
- the edge weights,
- or the accepting-state rule.

So this is a low-risk improvement, unlike a more radical redesign.

## 5. Optional next step after this lands

After the cached backend is in place, the next high-payoff optimization is a
**deterministic-child fast path**. On the resource family, every nontrivial child is
deterministic, so each obligation frontier is a singleton in practice. That can
shrink the hot path further.
