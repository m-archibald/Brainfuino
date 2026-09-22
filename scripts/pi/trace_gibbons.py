"""
Trace Gibbons algorithm step by step to see exact values of scalars and products.
"""
from test_stream_math import *

def trace_gibbons():
    q = 1
    r = 0
    t = 1
    k = 1
    l = 3
    n = 3
    step = 0
    digits = []
    while len(digits) < 10:
        step += 1
        cond = (4 * q + r) < (n + 1) * t
        print(f"--- Step {step} ---")
        print(f"q={q}, r={r}, t={t}, k={k}, l={l}, n={n}")
        print(f"Cond: 4q+r={4*q+r} < (n+1)t={(n+1)*t} -> {cond}")
        if cond:
            digits.append(str(n))
            print(f">>> EMIT: {n} (digits so far: {''.join(digits)})")
            r_diff = r - n * t
            r = 10 * r_diff
            n = (30 * q + r) // t
            q = 10 * q
            print(f"After emit: q={q}, r={r}, t={t}, n={n}")
        else:
            t = t * l
            r = (2 * q + r) * l
            n = (3 * k * q + r) // t
            q = q * k
            l += 2
            k += 1
            print(f"After step: q={q}, r={r}, t={t}, k={k}, l={l}, n={n}")

if __name__ == "__main__":
    trace_gibbons()
