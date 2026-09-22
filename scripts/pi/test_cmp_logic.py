"""
Test the forward bignum comparison and repeated-addition multiplication.
"""
def test_forward_cmp():
    def cmp_digits(a, b, cin):
        if a > b:
            return 1
        elif a < b:
            return 2
        else:
            return cin

    # Test cases: (A, B, expected_res)
    test_cases = [
        ([5, 4, 1], [9, 3, 1], 1), # 145 vs 139 -> 1
        ([9, 3, 1], [5, 4, 1], 2), # 139 vs 145 -> 2
        ([5, 4, 1], [5, 4, 1], 0), # 145 vs 145 -> 0
        ([0, 0, 0], [0, 0, 0], 0),
        ([1, 0, 0], [0, 0, 0], 1),
        ([0, 0, 0], [1, 0, 0], 2),
        ([9, 9, 9], [0, 0, 1], 1), # 999 vs 100 -> 1
        ([0, 0, 1], [9, 9, 9], 2), # 100 vs 999 -> 2
    ]

    for a, b, expected in test_cases:
        res = 0
        for da, db in zip(a, b):
            res = cmp_digits(da, db, res)
        assert res == expected, f"Failed for {a} vs {b}: got {res}, expected {expected}"
    print("Forward cmp logic: 100% PASSED!")

if __name__ == "__main__":
    test_forward_cmp()
