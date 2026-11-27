// EXPECT: error reference to the namespace "K" in "A" is ambiguous: it names Y::K and Z::K
// The same with A's two directives in the other order (Apple: ambiguous).
#include <metal_stdlib>
using namespace metal;
namespace Y { namespace K { constant int y = 1; } }
namespace Z { namespace K { constant int y = 2; } }
namespace D2 { using namespace Z; }
namespace A { using namespace D2; using namespace Y; }
kernel void qualified_namespace_ambiguous_through_nested_directive_reversed_rejected(device int* out [[buffer(0)]])
{ out[0] = A::K::y; }
