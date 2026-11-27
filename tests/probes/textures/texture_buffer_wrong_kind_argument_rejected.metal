// EXPECT: error which is not a texture_buffer<float, access::read>
// Apple: no viable conversion from texture1d to texture_buffer.
#include <metal_stdlib>
using namespace metal;
float4 f(texture_buffer<float, access::read> r) { return r.read(0u); }
kernel void k(texture1d<float, access::read> t [[texture(0)]], device float* o [[buffer(0)]]) { o[0] = f(t).x; }
