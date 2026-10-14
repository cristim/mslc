// EXPECT: error reference to the namespace "K" in "A" is ambiguous: it names Y::K and Z::K
// Y has a K of its own; D2 has none, so its directive is followed to Z's K. Each directive is
// searched on its own, so the result does not depend on their order (Apple: ambiguous).
#include <metal_stdlib>
using namespace metal;
namespace Y { namespace K { constant int y = 1; } }
namespace Z { namespace K { constant int y = 2; } }
namespace D2 { using namespace Z; }
namespace A { using namespace Y; using namespace D2; }
kernel void qualified_namespace_ambiguous_through_nested_directive_rejected(device int* out [[buffer(0)]])
{ out[0] = A::K::y; }
