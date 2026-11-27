// EXPECT: valid
// DISASM: OpFunctionCall
// DISASM-MATCH: OpImageFetch %v4uint
// DISASM: OpImageQuerySizeLod %v2uint
#include <metal_stdlib>
using namespace metal;
uint4 load(texture2d<uint, access::read> src, uint2 p) { return src.read(p) + uint4(src.get_width()); }
kernel void hit(texture2d<uint, access::read> a, device uint* out [[buffer(0)]]) {
  out[0] = load(a, uint2(1)).x;
}
