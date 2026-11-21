// EXPECT: error reference to "metal" is ambiguous
//
// Apple: "reference to 'metal' is ambiguous". The directive for A puts A::metal at file scope beside the library's metal.
#include <metal_stdlib>
namespace A { namespace metal { int abs(int a) { return 1007; } } }
using namespace A; using namespace metal;
int f(int x) { return x; }
kernel void using_namespace_metal_ambiguous_with_directive_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = f(int(i)); }
