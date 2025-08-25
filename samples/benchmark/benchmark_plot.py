import matplotlib.pyplot as plt

# Data
nwa_size = [2, 2, 2, 2, 3, 3, 4, 5]
bound =    [1, 2, 3, 4, 1, 2, 1, 1]
states =   [1873, 100369, 3386664, 25555, 2652801, 246161, 1946251]
qs =       [15, 25, 37, 22, 37, 29, 36]
success =  [True, True, False, True, False, True, True]

# Scatter: Q_S vs States
plt.figure(figsize=(8,6))
for i in range(len(states)):
    if qs[i] is not None:
        color = 'green' if success[i] else 'red'
        plt.scatter(qs[i], states[i], color=color, label=f'NWA {nwa_size[i]}, Bound {bound[i]}' if i==0 else "")
plt.xlabel('Q_S')
plt.ylabel('Number of States')
plt.title('Number of States vs Q_S')
plt.yscale('log')
plt.grid(True)
plt.savefig("states_vs_qs.png", bbox_inches='tight')
plt.show()

# Line: For each NWA size, plot States vs Bound
import numpy as np
plt.figure(figsize=(8,6))
for n in sorted(set(nwa_size)):
    x = [bound[i] for i in range(len(states)) if nwa_size[i]==n and qs[i] is not None]
    y = [states[i] for i in range(len(states)) if nwa_size[i]==n and qs[i] is not None]
    if x and y:
        plt.plot(x, y, marker='o', label=f'NWA size {n}')
plt.xlabel('Bound')
plt.ylabel('Number of States')
plt.yscale('log')
plt.title('Number of States vs Bound for each NWA size')
plt.legend()
plt.grid(True)
plt.savefig("states_vs_bound.png", bbox_inches='tight')
plt.show()

# Additional (n, k) data
qs_nk = [8, 34]
states_nk = [98, 680219]

# Combine, skipping None values
qs_all = [q for q in qs if q is not None] + qs_nk
states_all = [states[i] for i in range(len(states)) if qs[i] is not None] + states_nk

plt.figure(figsize=(8,6))
plt.scatter(qs_all, states_all, color='blue')
for i in range(len(qs_all)):
    plt.annotate(f'Q_S={qs_all[i]}, S={states_all[i]}', (qs_all[i], states_all[i]), textcoords="offset points", xytext=(0,8), ha='center', fontsize=8)
plt.xlabel('Q_S')
plt.ylabel('Number of States')
plt.yscale('log')
plt.title('Number of States vs Q_S (combined data)')
plt.grid(True)
plt.tight_layout()
plt.savefig("states_vs_qs_combined.png", bbox_inches='tight')
plt.show()