// EXPECT: valid
// DISASM-MATCH: OpImageQuerySizeLod %v2uint %[0-9A-Za-z_]+ %uint_1
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<half, access::read> src, device uint* out [[buffer(0)]]) { out[0] = src.get_width(1u); }
