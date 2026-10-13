// EXPECT: valid
// Regression (cross-agent review of #264, round 2): looksLikeType's statement-start test
// applied "a local hides a struct tag" to the resolved name regardless of qualification, so
// "::S s;" after a local "int S" was wrongly read as an expression (since the local shadowed
// the plain "S" test) instead of a declaration. A local hides the tag only for the bare,
// unqualified spelling; "::S" (or "A::S") still names the struct. Stores the local's 2 plus 5
// via the struct field.
#include <metal_stdlib>
using namespace metal;
struct S { int q; };
kernel void k(device int* out [[buffer(0)]]) {
	int S = 2;
	::S s;
	s.q = S + 5;
	out[0] = s.q;
}
