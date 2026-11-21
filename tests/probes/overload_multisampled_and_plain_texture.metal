// EXPECT: valid
// A texture2d_ms and a texture2d parameter are different types: the overloads coexist and each call picks its own (the plain one uses the default access, so only isMultisampled separates them).
// DISASM: OpFunctionCall
#include <metal_stdlib>
using namespace metal;
float4 g(texture2d_ms<float> t) { return t.read(uint2(0), 0); }
float4 g(texture2d<float> t) { return t.read(uint2(0)); }
fragment float4 hit(texture2d_ms<float> m [[texture(0)]], texture2d<float> c [[texture(1)]]) {
  return g(m) + g(c);
}
