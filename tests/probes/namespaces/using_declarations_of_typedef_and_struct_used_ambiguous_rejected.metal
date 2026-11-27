// EXPECT: error reference to "T" is ambiguous: it names A::T and B::T
// Regression (cross-agent review of #264, round 2): the mirror of
// using_declared_typedef_beside_struct_used_ambiguous_rejected with two using declarations
// instead of one own declaration. Unused, this is accepted
// (using_declarations_of_typedef_and_struct_unused_accepted); used, it stays ambiguous.
#include <metal_stdlib>
using namespace metal;
namespace A { typedef int T; }
namespace B { struct T { int q; }; }
using A::T;
using B::T;
kernel void k(device int* out [[buffer(0)]]) {
	T t = 1;
	out[0] = t;
}
