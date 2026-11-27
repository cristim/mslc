// EXPECT: error a long is not an index
//
// The element type counts as it does for a named constant: Apple takes an
// int or a uint for an attribute and rejects a long.
#include <metal_stdlib>
using namespace metal;
constexpr constant long remap[] = {0, 1};
struct Out { float4 a [[color(remap[0])]]; };
fragment Out attribute_index_long_constant_array_rejected()
{ Out o; o.a = float4(1.0); return o; }
