// EXPECT: error texture_buffer<char> is not lowered yet
// Apple: invalid channel type for texture_buffer.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<char, access::read> r [[texture(0)]], device float* o [[buffer(0)]]) { o[0] = 0; }
