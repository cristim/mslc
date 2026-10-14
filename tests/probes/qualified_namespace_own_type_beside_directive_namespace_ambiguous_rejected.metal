// EXPECT: error reference to the namespace "K" in "A" is ambiguous: it names D1::K and M::K
// D1's own enum K and M's namespace K are different entities A's directives each reach: a
// type found this way is not silently out-voted by a namespace of the same name through
// another directive, it joins the union and the two are ambiguous (Apple: "a type named
// 'K' is hidden by a declaration in a different namespace").
#include <metal_stdlib>
using namespace metal;
namespace D1 { enum K { x, y }; }
namespace M { namespace K { constant int y = 7; } }
namespace A { using namespace D1; using namespace M; }
kernel void qualified_namespace_own_type_beside_directive_namespace_ambiguous_rejected(device int* out [[buffer(0)]])
{ out[0] = A::K::y; }
