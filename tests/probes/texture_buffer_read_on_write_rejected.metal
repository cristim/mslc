// EXPECT: error write-only textures lower only
// Apple refuses this texture_buffer call too (no member named read).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::write> r [[texture(0)]], device float* o [[buffer(0)]]) { o[0] = r.read(0u).x; }
