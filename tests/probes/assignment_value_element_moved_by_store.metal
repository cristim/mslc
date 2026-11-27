// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// The value of an assignment to an element is loaded from the element stored to, not read by evaluating the
// operand again: a[a[0]] += 1 stores 1 to a[0] and is 1, where reading a[a[0]] again would give a[1], 5; and an
// index the right operand changes through a reference still names the element stored to (v = 10). Host clang
// gives 1, 1 and 10 for v1, a[0] and v2.
#include <metal_stdlib>
using namespace metal;
int bump(thread int& n) { n += 1; return n * 10; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	int a[3] = {0, 5, 6};
	int v1 = (a[a[0]] += 1);
	float arr[3] = {0, 0, 0};
	int k = 0;
	float v2 = (arr[k] = float(bump(k)));
	out[i] = float(v1) + float(a[0]) * 10.0f + v2 * 100.0f;
}
