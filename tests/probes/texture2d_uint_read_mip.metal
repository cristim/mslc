// EXPECT: valid
// DISASM-MATCH: OpImageFetch %v4uint %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %uint_0
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::read> src) { src.read(uint2(0), 0u); }
