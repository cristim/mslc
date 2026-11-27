// EXPECT: valid
// A half texture's fetched texel is narrowed to half4 like a sampled one.
// DISASM-MATCH: OpImageFetch %v4float
// DISASM-MATCH: OpFConvert %v4half %[_0-9a-zA-Z]+
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<half> t [[texture(0)]]) {
  return float4(t.read(uint2(1, 2)));
}
