// EXPECT: valid
// Apple takes a ushort2 coordinate; the fetch takes the 32-bit form.
// DISASM-MATCH: OpUConvert %v2uint %[_0-9a-zA-Z]+[^_0-9a-zA-Z]
// DISASM-MATCH: OpImageFetch %v4float
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.read(ushort2(1, 2));
}
