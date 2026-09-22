"""
test_stream_math.py - Byte-level simulation of Gibbons LFT Spigot algorithm
Matches the exact register layout and arithmetic constraints of our Brainfuck engine.
"""

from typing import List, Tuple

# Word size: 8-bit bytes (0..255)
BASE = 256

def bn_from_int(val: int, length: int) -> List[int]:
    """Convert non-negative int to little-endian byte array."""
    res = []
    v = abs(val)
    for _ in range(length):
        res.append(v & 0xFF)
        v >>= 8
    assert v == 0, f"Value {val} overflows length {length}"
    return res

def bn_to_int(arr: List[int]) -> int:
    val = 0
    for b in reversed(arr):
        val = (val << 8) | b
    return val

def bn_cmp(a: List[int], b: List[int]) -> int:
    """Compare magnitudes: return 1 if a > b, -1 if a < b, 0 if a == b."""
    for i in range(len(a) - 1, -1, -1):
        if a[i] > b[i]:
            return 1
        elif a[i] < b[i]:
            return -1
    return 0

def bn_add(a: List[int], b: List[int]) -> List[int]:
    """Return a + b with ripple carry."""
    res = [0] * len(a)
    carry = 0
    for i in range(len(a)):
        total = a[i] + b[i] + carry
        res[i] = total & 0xFF
        carry = total >> 8
    return res

def bn_sub(a: List[int], b: List[int]) -> List[int]:
    """Return a - b assuming a >= b."""
    assert bn_cmp(a, b) >= 0, "bn_sub requires a >= b"
    res = [0] * len(a)
    borrow = 0
    for i in range(len(a)):
        diff = a[i] - b[i] - borrow
        if diff < 0:
            diff += 256
            borrow = 1
        else:
            borrow = 0
        res[i] = diff
    assert borrow == 0
    return res

def bn_mul_scalar(a: List[int], scalar: int) -> List[int]:
    """Return a * scalar where scalar fits in 16 bits."""
    res = [0] * len(a)
    carry = 0
    for i in range(len(a)):
        prod = a[i] * scalar + carry
        res[i] = prod & 0xFF
        carry = prod >> 8
    assert carry == 0, "Multiplication overflow"
    return res

def bn_div_single(num: List[int], den: List[int]) -> Tuple[int, List[int]]:
    """
    Computes q = num // den where q in 0..9 via repeated subtraction.
    Returns (q, remainder).
    """
    q = 0
    rem = list(num)
    while bn_cmp(rem, den) >= 0:
        rem = bn_sub(rem, den)
        q += 1
        assert q <= 9, f"Quotient exceeded 9: {q}"
    return q, rem


def simulate_gibbons_bytes(num_digits: int = 50, reg_len: int = 128) -> str:
    """
    Simulate Gibbons algorithm using purely byte-array operations.
    State:
      q: unsigned bignum
      r_sign: 0 for +, 1 for -
      r_mag: unsigned bignum
      t: unsigned bignum
      k: int (16-bit)
      l: int (16-bit)
      n: int (0..9)
    """
    q = bn_from_int(1, reg_len)
    r_sign = 0
    r_mag = bn_from_int(0, reg_len)
    t = bn_from_int(1, reg_len)
    k = 1
    l = 3
    n = 3

    digits = []
    steps = 0
    max_steps = num_digits * 20

    while len(digits) < num_digits and steps < max_steps:
        steps += 1
        # Condition check: 4*q + r < (n + 1)*t
        # If r >= 0: LHS = 4*q + r_mag, RHS = (n + 1)*t
        # If r < 0:  LHS = 4*q,         RHS = (n + 1)*t + r_mag
        four_q = bn_mul_scalar(q, 4)
        n_plus_1_t = bn_mul_scalar(t, n + 1)

        if r_sign == 0:
            lhs = bn_add(four_q, r_mag)
            rhs = n_plus_1_t
        else:
            lhs = four_q
            rhs = bn_add(n_plus_1_t, r_mag)

        condition = (bn_cmp(lhs, rhs) < 0)

        if condition:
            # Emit branch
            digits.append(str(n))
            # r_new = 10 * (r - n * t)
            # Compute r - n*t:
            n_t = bn_mul_scalar(t, n)
            if r_sign == 0:
                # r is positive
                if bn_cmp(r_mag, n_t) >= 0:
                    r_diff_mag = bn_sub(r_mag, n_t)
                    r_diff_sign = 0
                else:
                    r_diff_mag = bn_sub(n_t, r_mag)
                    r_diff_sign = 1
            else:
                # r is negative: -|r| - n*t = -(|r| + n*t)
                r_diff_mag = bn_add(r_mag, n_t)
                r_diff_sign = 1

            # r = 10 * r_diff
            r_mag = bn_mul_scalar(r_diff_mag, 10)
            r_sign = r_diff_sign

            # n_new = (30*q + r) // t
            # Compute 30*q + r:
            thirty_q = bn_mul_scalar(q, 30)
            if r_sign == 0:
                num = bn_add(thirty_q, r_mag)
            else:
                # thirty_q - |r| (assert thirty_q >= |r|)
                assert bn_cmp(thirty_q, r_mag) >= 0, "30*q < |r| in emit branch"
                num = bn_sub(thirty_q, r_mag)

            n_new, _ = bn_div_single(num, t)
            q = bn_mul_scalar(q, 10)
            n = n_new

        else:
            # Step branch (consume term)
            # t = t * l
            t = bn_mul_scalar(t, l)

            # Compute (2*q + r):
            two_q = bn_mul_scalar(q, 2)
            if r_sign == 0:
                two_q_plus_r_mag = bn_add(two_q, r_mag)
                two_q_plus_r_sign = 0
            else:
                if bn_cmp(two_q, r_mag) >= 0:
                    two_q_plus_r_mag = bn_sub(two_q, r_mag)
                    two_q_plus_r_sign = 0
                else:
                    two_q_plus_r_mag = bn_sub(r_mag, two_q)
                    two_q_plus_r_sign = 1

            # r_new = (2*q + r) * l
            r_mag = bn_mul_scalar(two_q_plus_r_mag, l)
            r_sign = two_q_plus_r_sign

            # n_new = (3*k*q + r) // t  (using the new t = t*l)
            three_k_q = bn_mul_scalar(q, 3 * k)
            if r_sign == 0:
                num = bn_add(three_k_q, r_mag)
            else:
                assert bn_cmp(three_k_q, r_mag) >= 0, "3*k*q < |r| in step branch"
                num = bn_sub(three_k_q, r_mag)

            n_new, _ = bn_div_single(num, t)

            # q = q * k
            q = bn_mul_scalar(q, k)
            l += 2
            k += 1
            n = n_new

    return "".join(digits)


if __name__ == "__main__":
    KNOWN_50 = "31415926535897932384626433832795028841971693993751"
    print("Simulating Gibbons algorithm with byte-level bignums (reg_len=128)...")
    res = simulate_gibbons_bytes(50, reg_len=128)
    print("Computed:", res)
    print("Expected:", KNOWN_50)
    print("Match?   ", res == KNOWN_50)
    assert res == KNOWN_50, "Byte-level simulation mismatch!"
    print("ALL TESTS PASSED!")
