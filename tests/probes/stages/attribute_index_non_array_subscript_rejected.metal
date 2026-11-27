// EXPECT: error is not a constant integer array
//
// Only a const or constexpr array of integers can be indexed in a constant
// expression: an integer constant is not one.
#include <metal_stdlib>
using namespace metal;
constant uint scalar_value = 1;
struct Out { float4 a [[color(scalar_value[0])]]; };
fragment Out attribute_index_non_array_subscript_rejected()
{ Out o; o.a = float4(1.0); return o; }
