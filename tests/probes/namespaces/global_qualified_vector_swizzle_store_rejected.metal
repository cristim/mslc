// EXPECT: error cannot store to "kv", a constant declared at file scope
#include <metal_stdlib>
using namespace metal;
constant float4 kv = { 1.0f, 2.0f, 3.0f, 4.0f };
kernel void kern(device float* out [[buffer(0)]]) {
	float4 kv = float4(0.0f);
	::kv.xy = 5.0f;
	out[0] = kv.x;
}
