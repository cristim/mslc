// EXPECT: error has read, write and get_width only
// Apple accepts this (texture atomics on a texture_buffer); mslc refuses it by name.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<uint, access::read_write> r [[texture(0)]], device float* o [[buffer(0)]]) { r.atomic_fetch_add(0u, 1u); }
