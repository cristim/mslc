// EXPECT: error reference to "metal" is ambiguous
//
// Apple: the same ambiguity from inside a namespace that nominates A, using metal::abs.
#include <metal_stdlib>
namespace A { namespace metal { int abs(int a) { return 1007; } } }
namespace B { using namespace A; int f(int x) { return metal::abs(x); } }
kernel void nominating_directive_in_namespace_makes_metal_ambiguous_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = B::f(int(i)); }
