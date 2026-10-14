// EXPECT: error reference to the namespace "K" in "A" is ambiguous: it names Y::K and Z::K
// Two of A's directives each reach a K (Apple: "reference to 'K' is ambiguous").
#include <metal_stdlib>
using namespace metal;
namespace Y { namespace K { constant int y = 1; } }
namespace Z { namespace K { constant int y = 2; } }
namespace A { using namespace Y; using namespace Z; }
kernel void qualified_namespace_through_two_directives_ambiguous_rejected(device int* out [[buffer(0)]])
{ out[0] = A::K::y; }
