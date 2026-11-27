// EXPECT: error reference to the namespace "K" in "A" is ambiguous: it names D::K and M
#include <metal_stdlib>
using namespace metal;
namespace M { constant int y = 7; }
namespace D { enum K { y = 1 }; }
namespace A { namespace K = M; using D::K; }
kernel void namespace_alias_enum_before_using_type_ambiguous_rejected(device int* out [[buffer(0)]])
{ out[0] = int(A::K::y); }
