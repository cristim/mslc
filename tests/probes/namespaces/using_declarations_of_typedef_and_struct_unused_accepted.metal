// EXPECT: valid
// Regression (cross-agent review of #264): two using declarations of one alias, a typedef
// and a struct, are not a clash (the clash lambda in parseUsing uses the widened tag-beside-
// any-kind exemption, not only tag-beside-object). Unused: Apple reports the alias ambiguous
// only where it is actually read (an existing, unrelated limitation this PR does not change).
#include <metal_stdlib>
using namespace metal;
namespace A { typedef int T; }
namespace B { struct T { int q; }; }
using A::T;
using B::T;
kernel void k(device int* out [[buffer(0)]]) {
	out[0] = 1;
}
