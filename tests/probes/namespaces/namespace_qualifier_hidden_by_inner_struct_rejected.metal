// EXPECT: error "N::K::y" is not a constant
//
// Apple: "no member named 'y' in 'N::K'". The struct hides the outer namespace K, and a struct has no member y.
#include <metal_stdlib>
using namespace metal;
namespace K { constant int y = 1; }
namespace N { struct K { int a; }; constant int z = K::y; }
kernel void namespace_qualifier_hidden_by_inner_struct_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = N::z; }
