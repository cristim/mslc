// EXPECT: valid
// DISASM-MATCH: OpIAdd %int %int_20[_0-9]* %int_20
// A and B use each other and only C, which B also uses, has a K: the walk does not revisit
// a namespace it has seen, and goes on to C (Apple accepts).
#include <metal_stdlib>
using namespace metal;
namespace C { namespace K { constant int y = 20; } }
namespace A { }
namespace B { using namespace A; using namespace C; }
namespace A { using namespace B; }
kernel void qualified_namespace_through_cyclic_directives(device int* out [[buffer(0)]])
{ out[0] = A::K::y + B::K::y; }
