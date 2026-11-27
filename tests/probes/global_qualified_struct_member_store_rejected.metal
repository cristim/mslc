// EXPECT: error cannot store to "ks", a constant declared at file scope
#include <metal_stdlib>
using namespace metal;
struct S { float x; };
constant S ks = { .x = 1.0f };
kernel void kern(device float* out [[buffer(0)]]) {
	S ks = { .x = 2.0f };
	::ks.x = 5.0f;
	out[0] = ks.x;
}
