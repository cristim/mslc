// EXPECT: valid
// Regression (cross-agent review of #264, round 2): the same qualified-spelling guard applies
// to the for-loop initialiser's type test, not only the statement-start one.
#include <metal_stdlib>
using namespace metal;
struct S { int q; };
kernel void k(device int* out [[buffer(0)]]) {
	int S = 2;
	for (::S s = {0}; s.q < 1; s.q++) {
		out[0] = s.q;
	}
	out[0] = S;
}
