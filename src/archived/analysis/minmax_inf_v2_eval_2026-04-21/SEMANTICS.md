# Semantics

This evaluation uses the following explicit rule for one-state children.

Rule:
- A one-state child is treated as silent if its unique state is final.
- A one-state child is not allowed if its unique state is non-final.

Interpretation:
- "Silent" means the parent edge behaves like an edge with no live child
  obligation: the Min/Max `Inf`/`LimInf` backend should not spawn a tracked
  child call for that edge.
- "Not allowed" means such an input is outside the intended well-formedness
  assumptions for these experiments.

Status in the current codebase:
- This rule is recorded here as the intended semantics for analysis.
- This evaluation does not attempt to patch every other code path to enforce the
  rule globally.
- Any observed behavior on non-final one-state children should therefore be read
  as diagnostic behavior of the current implementation, not as the accepted
  target semantics.
