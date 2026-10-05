// EXPECT: valid
// read(coord, lod) passes the lod, converted to uint, to the fetch.
// DISASM-MATCH: OpImageFetch %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %uint_1(_[0-9])?[^_0-9a-zA-Z]
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.read(uint2(1, 2), 1u);
}
