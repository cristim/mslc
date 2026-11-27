// EXPECT: valid
// Regression (cross-agent review of #264): two using declarations of one alias, an enumerator
// and a struct, are not a clash either (the widened tag exemption covers an enum constant too,
// not only a typedef). Unused, for the same reason as using_declarations_of_typedef_and_
// struct_unused_accepted.
#include <metal_stdlib>
using namespace metal;
namespace A { enum { V }; }
namespace B { struct V { int q; }; }
using A::V;
using B::V;
kernel void k(device int* out [[buffer(0)]]) {
	out[0] = 1;
}
