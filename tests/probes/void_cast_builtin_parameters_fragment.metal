// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
fragment float4 f(float4 p [[position]], bool ff [[front_facing]]) { (void)p; (void)ff; return float4(1); }
