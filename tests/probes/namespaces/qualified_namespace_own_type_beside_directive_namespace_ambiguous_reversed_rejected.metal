// EXPECT: error reference to the namespace "K" in "A" is ambiguous: it names D1::K and M::K
// The same with A's two directives in the other order (Apple: ambiguous either way).
#include <metal_stdlib>
using namespace metal;
namespace D1 { enum K { x, y }; }
namespace M { namespace K { constant int y = 7; } }
namespace A { using namespace M; using namespace D1; }
kernel void qualified_namespace_own_type_beside_directive_namespace_ambiguous_reversed_rejected(device int* out [[buffer(0)]])
{ out[0] = A::K::y; }
