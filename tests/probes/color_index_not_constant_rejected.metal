// EXPECT: error needs a constant integer argument
//
// An identifier that is not an enumerator is no index, the same rule
// attribute_index_not_constant_rejected.metal pins for [[attribute(n)]].
#include <metal_stdlib>
using namespace metal;
struct Out { float4 a [[color(slot)]]; };
fragment Out color_index_not_constant_rejected()
{ Out o; o.a = float4(1.0); return o; }
