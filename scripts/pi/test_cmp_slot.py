"""
Verify full cmp_slot logic.
"""
def cmp_slot(a, b, cin):
    is_gt = (a > b)
    is_lt = (b > a)
    if is_gt:
        return 1
    elif is_lt:
        return 2
    else:
        return cin

def test_full():
    for a in range(10):
        for b in range(10):
            for cin in [0, 1, 2]:
                cout = cmp_slot(a, b, cin)
                if a > b: assert cout == 1
                elif a < b: assert cout == 2
                else: assert cout == cin
    print("ALL 300 CASES PASSED!")

test_full()
