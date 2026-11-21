// EXPECT: error reference to "metal" is ambiguous
//
// Apple: metal::abs names metal, which is the library's and A::metal through the directive.
#include <metal_stdlib>
namespace A { namespace metal { int abs(int a) { return 1007; } } }
using namespace A;
int f(int x) { return metal::abs(x); }
kernel void qualified_metal_ambiguous_with_directive_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = f(int(i)); }
