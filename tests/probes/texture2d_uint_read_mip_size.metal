// EXPECT: valid
// DISASM-MATCH: OpImageQuerySizeLod %v2uint %[0-9A-Za-z_]+ %uint_1
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::read> src, device uint* o [[buffer(0)]]) { o[0] = src.get_width(1u); }
