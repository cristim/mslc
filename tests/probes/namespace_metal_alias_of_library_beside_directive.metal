// EXPECT: valid
// DISASM: SAbs
//
// Apple: A::metal is an alias of the library's namespace, the same entity, so the directive for A makes nothing ambiguous.
#include <metal_stdlib>
namespace A { namespace metal = ::metal; }
using namespace A;
int f(int x) { return metal::abs(x); }
kernel void namespace_metal_alias_of_library_beside_directive(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = f(int(i)); }
