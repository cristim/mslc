// EXPECT: error no member named "f" in the namespace "N"
// A using directive in a function of N does not make A's names members of N
// (Apple: "no member named 'f' in namespace 'N'").
#include <metal_stdlib>
using namespace metal;
namespace A { int f() { return 11; } }
namespace N { int g() { using namespace A; return N::f(); } }
kernel void using_directive_in_block_not_a_namespace_member_rejected(device int* out [[buffer(0)]])
{ out[0] = N::g(); }
