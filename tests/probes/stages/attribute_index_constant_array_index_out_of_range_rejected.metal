// EXPECT: error is outside the array
//
// An element past the end of a constant array is no constant, so it is no
// attribute index either (Apple refuses the call to a constant expression
// that reads out of bounds).
#include <metal_stdlib>
using namespace metal;
constexpr constant uint remap[] = {0, 1};
struct Out { float4 a [[color(remap[2])]]; };
fragment Out attribute_index_constant_array_index_out_of_range_rejected()
{ Out o; o.a = float4(1.0); return o; }
