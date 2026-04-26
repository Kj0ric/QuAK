# Key Lemma Flattening Sketch

Historical note: this sketch predates the current explicit final-state handling
and should be read as design context, not as current parser or API
documentation.

## PRELIMINARIES ##

/* Let the nested weighted automaton be
     AA = ⟨ A_mas ; f ;  B₁, …, B_k ⟩                */

n ← max{|w| : w is a weight occurring in any Bi}   // global weight bound -- we can do this tigther for each automaton if we have minDomain and maxDomain correctly precomputed
Σ ← input alphabet (same for master and all slaves)


## DETERMINISTIC MONITORS FOR SLAVES ##

Q_S  ← ∅            // global pool of monitor states (pairwise disjoint)
F_S  ← ∅            // accepting states of those monitors

for i in 1..k:                       // iterate over the k slave automata
    for j in −n .. n:                // every possible integer return value
        S_i^j ←  Determinise(  Bi restricted to value j  )
        /* S_i^j accepts precisely the finite words on which Bi
           terminates with return value j                        */
        Q_S ← Q_S ∪ States(S_i^j)      // *disjoint* union (rename if necessary)
        F_S ← F_S ∪ AcceptingStates(S_i^j)


## STEPPING ALL CURRENTLY ACTIVE MONITORS IN PARALLEL ##

function STEP(M : set of Q_S, a : Σ) → set of Q_S:
    /* Advance every monitor in M reading one letter a              */
    return { q' ∈ Q_S |   ∃q∈M  s.t. (q, a, q') is a transition of its automaton }


## STATE SPACE OF THE FLATTENED AUTOMATON BB ##

Q′  ←   Q_mas                    // current state of the master automaton
         × ( {⊥} ∪ [−n,n] )      // last *guessed* return value (⊥ = no guess)
         × 2^{Q_S}               // P₁ : monitors started in the *current* epoch
         × 2^{Q_S}               // P₂ : monitors still to be finished from *earlier* epochs

q₀′ ←  ( q₀_mas , ⊥ , ∅ , ∅ )

Meaning of the components:
⊥ / [−n,n] : the value placed on the previous transition (needed by the value function sil(f)).
P₁ : monitors spawned in the current epoch (see below). They all must finish before the master issues another call transition that closes the epoch.
P₂ : monitors left over from earlier epochs. They may continue running, but the acceptance condition will require that they all eventually finish, making P₂ empty infinitely often.

Because all master states are accepting, BB will use a single Büchi set

F'  ←   Q_mas  × ( {⊥}∪[−n,n] ) × 2^{Q_S} × {∅}

A run is accepting iff it visits states with an empty P₂ infinitely many times.

## TRANSITION RELATION OF BB ##

Let a transition of the master automaton be

(q_mas,      a, q_mas',   label)

where label = i identifies the invoked slave Bi, or label = silent if the master emits no call at this step.

For every global state (q,j,P₁,P₂) and every input letter a∈Σ we add transitions as follows:

/* First, advance all active monitors one step over a */
P₁next ← STEP(P₁, a)   \  F_S      // remove the ones that have just accepted
P₂next ← STEP(P₂, a)   \  F_S

if label == silent  then               // --------  CASE (A)  --------
    add transition
        ( (q,j,P₁,P₂)  ─a/⊥→  ( q', ⊥, P₁next,       P₂next ) )

else                                    // --------  CASE (B/C)  --------
    /* The master invokes slave Bi */
    for each guess in −n .. n:          // choose the return value to emit now
        init ← initial( S_i^guess )

        if P₂ == ∅ then                 //   CASE (B):  *start a fresh epoch*
            P₁new ← { init }            // monitors of the new epoch
            P₂new ← P₁next             // monitors from the closing epoch
        else                            //   CASE (C):  *overlapping call*
            P₁new ← ( P₁next ∪ {init} ) \ F_S
            P₂new ← P₂next             // still non‑empty

        add transition
            ( (q,j,P₁,P₂) ─a/guess→  ( q', guess, P₁new, P₂new ) )

The weight carried by each transition is shown after the slash (⊥ for silent steps, the integer guess otherwise).  This realises the silenced value function sil(f).
