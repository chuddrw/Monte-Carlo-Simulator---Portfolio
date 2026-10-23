#!/bin/bash

# test_monte_carlo.sh
# Tests the monte_carlo simulation with various formulas, sample sizes, and distributions.
# Each test pipes input automatically so no manual entry is needed.
# Expected approximate values are noted in comments for sanity checking.

PASS=0
FAIL=0

run_test() {
    local test_name="$1"
    local input="$2"
    local expected_pattern="$3"  # regex pattern to match in output

    echo "----------------------------------------"
    echo "TEST: $test_name"
    echo "Input:"
    echo "$input" | sed 's/^/  /'

    output=$(echo "$input" | ./monte_carlo 2>/dev/null)
    echo "Output: $output"

    if echo "$output" | grep -qE "$expected_pattern"; then
        echo "RESULT: PASS"
        PASS=$((PASS + 1))
    else
        echo "RESULT: FAIL (expected pattern: $expected_pattern)"
        FAIL=$((FAIL + 1))
    fi
    echo ""
}

echo "========================================"
echo "  Monte Carlo Simulation Test Suite"
echo "========================================"
echo ""

# -----------------------------------------------------------------------
# TEST 1: x*y+z, all Uniform(0,1), N=10000
# E[x*y+z] = E[x]*E[y] + E[z] = 0.5*0.5 + 0.5 = 0.75
# Expect estimate close to 0.75
# -----------------------------------------------------------------------
run_test "x*y+z | Uniform(0,1) x3 | N=10000" \
"x*y+z
1
1
1
10000" \
"Estimate E\[f\] = 0\.[67][0-9]"

# -----------------------------------------------------------------------
# TEST 2: single variable x, Uniform(0,1), N=100 (small N, more variance)
# E[x] = 0.5, but with only 100 samples expect wider range
# -----------------------------------------------------------------------
run_test "x | Uniform(0,1) | N=100 (small sample)" \
"x
1
100" \
"Estimate E\[f\] = 0\.[0-9]"

# -----------------------------------------------------------------------
# TEST 3: single variable x, Uniform(0,1), N=1000000 (large N, should be very close to 0.5)
# E[x] = 0.5
# -----------------------------------------------------------------------
run_test "x | Uniform(0,1) | N=1000000 (large sample)" \
"x
1
1000000" \
"Estimate E\[f\] = 0\.4[5-9][0-9]|Estimate E\[f\] = 0\.5[0-4][0-9]"

# -----------------------------------------------------------------------
# TEST 4: x+y, both Uniform(0,1), N=50000
# E[x+y] = E[x] + E[y] = 0.5 + 0.5 = 1.0
# -----------------------------------------------------------------------
run_test "x+y | Uniform(0,1) x2 | N=50000" \
"x+y
1
1
50000" \
"Estimate E\[f\] = 0\.9[0-9]|Estimate E\[f\] = 1\.0[0-9]"

# -----------------------------------------------------------------------
# TEST 5: x*x, Uniform(0,1), N=10000
# E[x^2] = integral of x^2 from 0 to 1 = 1/3 ≈ 0.333
# -----------------------------------------------------------------------
run_test "x*x | Uniform(0,1) | N=10000" \
"x*x
1
10000" \
"Estimate E\[f\] = 0\.[23][0-9]"

# -----------------------------------------------------------------------
# TEST 6: x, Normal(0, 1), N=10000
# E[x] = 0 (mean of standard normal)
# Expect close to 0, could be slightly negative or positive
# -----------------------------------------------------------------------
run_test "x | Normal(mean=0, std=1) | N=10000" \
"x
3
0
1
10000" \
"Estimate E\[f\] = -0\.[0-2][0-9]|Estimate E\[f\] = 0\.[0-2][0-9]"

# -----------------------------------------------------------------------
# TEST 7: x, Exponential(lambda=1), N=10000
# E[x] = 1/lambda = 1.0
# -----------------------------------------------------------------------
run_test "x | Exponential(lambda=1) | N=10000" \
"x
4
1
10000" \
"Estimate E\[f\] = 0\.[89][0-9]|Estimate E\[f\] = 1\.[01][0-9]"

# -----------------------------------------------------------------------
# TEST 8: x, Bernoulli(p=0.5), N=10000
# E[x] = p = 0.5
# -----------------------------------------------------------------------
run_test "x | Bernoulli(p=0.5) | N=10000" \
"x
5
0.5
10000" \
"Estimate E\[f\] = 0\.[34567][0-9]"

# -----------------------------------------------------------------------
# TEST 9: x+y+z+w+v, all Uniform(0,1), N=10000 (max variables)
# E[x+y+z+w+v] = 5 * 0.5 = 2.5
# -----------------------------------------------------------------------
run_test "x+y+z+w+v | Uniform(0,1) x5 | N=10000 (max variables)" \
"x+y+z+w+v
1
1
1
1
1
10000" \
"Estimate E\[f\] = 2\.[34][0-9]|Estimate E\[f\] = 2\.[56][0-9]"

# -----------------------------------------------------------------------
# TEST 10: x, Uniform(0,1), N=1 (minimum samples edge case)
# Should return something between 0 and 1
# -----------------------------------------------------------------------
run_test "x | Uniform(0,1) | N=1 (minimum samples)" \
"x
1
1" \
"Estimate E\[f\] = 0\.[0-9]"

# -----------------------------------------------------------------------
# TEST 11: x*y, Uniform(a=2, b=4), N=10000
# E[x] = (2+4)/2 = 3, E[y] = 3, E[x*y] = E[x]*E[y] = 9.0
# -----------------------------------------------------------------------
run_test "x*y | Uniform(a=2,b=4) x2 | N=10000" \
"x*y
2
2
4
2
2
4
10000" \
"Estimate E\[f\] = [89]\.[0-9]"

# -----------------------------------------------------------------------
# TEST 12: x, Poisson(lambda=3), N=10000
# E[x] = lambda = 3.0
# -----------------------------------------------------------------------
run_test "x | Poisson(lambda=3) | N=10000" \
"x
7
3
10000" \
"Estimate E\[f\] = 2\.[89][0-9]|Estimate E\[f\] = 3\.[01][0-9]"

# -----------------------------------------------------------------------
# TEST 13: x^2, Uniform(0,1), N=10000
# E[x^2] = integral of x^2 from 0 to 1 = 1/3 ≈ 0.333
# Tests the ^ operator
# -----------------------------------------------------------------------
run_test "x^2 | Uniform(0,1) | N=10000 (exponent operator)" \
"x^2
1
10000" \
"Estimate E\[f\] = 0\.[23][0-9]"

# -----------------------------------------------------------------------
# TEST 14: (x+y)^2, both Uniform(0,1), N=10000
# E[(x+y)^2] = E[x^2 + 2xy + y^2] = 1/3 + 2*(1/4) + 1/3 = 1/3 + 1/2 + 1/3 ≈ 1.167
# Tests ^ combined with brackets
# -----------------------------------------------------------------------
run_test "(x+y)^2 | Uniform(0,1) x2 | N=10000 (brackets + exponent)" \
"(x+y)^2
1
1
10000" \
"Estimate E\[f\] = 1\.[01][0-9]"

# -----------------------------------------------------------------------
# TEST 15: (x+y)*(x-y), both Uniform(0,1), N=10000
# E[(x+y)*(x-y)] = E[x^2 - y^2] = 1/3 - 1/3 = 0.0
# Tests brackets with subtraction and multiplication, result near zero
# -----------------------------------------------------------------------
run_test "(x+y)*(x-y) | Uniform(0,1) x2 | N=10000 (brackets, result near 0)" \
"(x+y)*(x-y)
1
1
10000" \
"Estimate E\[f\] = -0\.[01][0-9]|Estimate E\[f\] = 0\.[01][0-9]"

# -----------------------------------------------------------------------
# TEST 16: x/y, x=Uniform(0,1), y=Uniform(2,4), N=10000
# E[x/y] ≈ E[x] * E[1/y]; for Uniform(2,4): E[1/y] = ln(4/2)/(4-2) = ln(2)/2 ≈ 0.347
# So E[x/y] ≈ 0.5 * 0.347 ≈ 0.173
# Tests division operator, y kept away from 0 to avoid divide-by-zero
# -----------------------------------------------------------------------
run_test "x/y | x=Uniform(0,1), y=Uniform(2,4) | N=10000 (division)" \
"x/y
1
2
2
4
10000" \
"Estimate E\[f\] = 0\.[01][0-9]"

# -----------------------------------------------------------------------
# TEST 17: (x+y)/z, all Uniform(1,3), N=10000
# E[x+y] = 2+2 = 4, E[1/z] for Uniform(1,3) = ln(3)/2 ≈ 0.549
# E[(x+y)/z] ≈ 4 * 0.549 ≈ 2.197  (independence doesn't hold exactly but approx)
# Tests division with brackets, z kept away from 0
# -----------------------------------------------------------------------
run_test "(x+y)/z | Uniform(1,3) x3 | N=10000 (brackets + division)" \
"(x+y)/z
2
1
3
2
1
3
2
1
3
10000" \
"Estimate E\[f\] = [12]\.[0-9]"

# -----------------------------------------------------------------------
# TEST 18: x^3, Uniform(0,1), N=10000
# E[x^3] = integral of x^3 from 0 to 1 = 1/4 = 0.25
# Tests higher exponent
# -----------------------------------------------------------------------
run_test "x^3 | Uniform(0,1) | N=10000 (higher exponent)" \
"x^3
1
10000" \
"Estimate E\[f\] = 0\.[12][0-9]"

# -----------------------------------------------------------------------
# TEST 19: ((x+y)*z)^2, all Uniform(0,1), N=10000
# Deeply nested brackets with exponent
# E[((x+y)*z)^2] = E[(xz+yz)^2] — complex, but result should be positive
# Just check it runs and produces a positive finite number
# -----------------------------------------------------------------------
run_test "((x+y)*z)^2 | Uniform(0,1) x3 | N=10000 (nested brackets + exponent)" \
"((x+y)*z)^2
1
1
1
10000" \
"Estimate E\[f\] = 0\.[0-9]"

# -----------------------------------------------------------------------
# TEST 20: x/x, Uniform(1,3), N=1000
# E[x/x] = E[1] = 1.0 exactly (as long as x != 0, which Uniform(1,3) guarantees)
# Good sanity check for division correctness
# -----------------------------------------------------------------------
run_test "x/x | Uniform(1,3) | N=1000 (division sanity: should be exactly 1)" \
"x/x
2
1
3
1000" \
"Estimate E\[f\] = 1\.0"

# -----------------------------------------------------------------------
# SUMMARY
# -----------------------------------------------------------------------
echo "========================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "========================================"

if [ $FAIL -eq 0 ]; then
    exit 0
else
    exit 1
fi