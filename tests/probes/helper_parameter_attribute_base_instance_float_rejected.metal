// EXPECT: error has an attribute, [[base_instance]], that its type float is not valid for
// QUIRK (helper parameter attributes): Apple's compiler reports "type 'float' is not valid for attribute
// 'base_instance'" on a helper.
#include <metal_stdlib>
using namespace metal;
float f(float b [[base_instance]]) { return b; }
vertex float4 v() { return float4(f(1.0f)); }
