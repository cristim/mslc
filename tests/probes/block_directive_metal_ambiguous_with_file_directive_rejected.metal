// EXPECT: error reference to "metal" is ambiguous
//
// Apple: the same ambiguity when the directives are in the function body.
#include <metal_stdlib>
namespace A { namespace metal { int abs(int a) { return 1007; } } }
int f(int x) { using namespace A; using namespace metal; return abs(x); }
kernel void block_directive_metal_ambiguous_with_file_directive_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = f(int(i)); }
