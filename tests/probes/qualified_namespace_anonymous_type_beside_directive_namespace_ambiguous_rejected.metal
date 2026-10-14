// EXPECT: error reference to the namespace "K" in "A" is ambiguous: it names A::(anonymous namespace)::K and M::K
// A's own anonymous namespace declares an enum K (a directive of A, same as a using
// directive, for this walk); M's namespace K is reached through a using directive. The two
// are different entities and ambiguous, the same as two using directives would be.
#include <metal_stdlib>
using namespace metal;
namespace A { namespace { enum K { x, y }; } }
namespace M { namespace K { constant int y = 7; } }
namespace A { using namespace M; }
kernel void qualified_namespace_anonymous_type_beside_directive_namespace_ambiguous_rejected(device int* out [[buffer(0)]])
{ out[0] = A::K::y; }
