// EXPECT: error "N::K::y" is not a parameter, local, constant or builtin mslc knows about
//
// Apple: "no member named y in N::K": the struct hides namespace K.
#include <metal_stdlib>
using namespace metal;
namespace K { constant int y = 1; }
namespace N { struct K { int a; }; }
int g(int x) { using N::K; return K::y; }
kernel void block_using_struct_hides_outer_namespace_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = g(int(i)); }
