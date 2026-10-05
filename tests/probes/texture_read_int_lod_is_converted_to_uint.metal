// EXPECT: valid
// Apple takes read(coord, 1) with an int lod; the fetch takes a uint.
// DISASM-MATCH: OpBitcast %uint %[_0-9a-zA-Z]+[^_0-9a-zA-Z]
// DISASM-MATCH: OpImageFetch %v4float
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  int l = 1;
  return t.read(uint2(1, 2), l);
}
