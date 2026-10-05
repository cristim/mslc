// EXPECT: valid
// A texture2d<half> has the same image type as a float one: Vulkan has no 16-bit
// sampled type for an image, so the lookup is a float4 and is narrowed to half4
// afterwards (Iridium does the same, air.cpp:753-761). The half is a claim about the
// shader's result and not about the image the app binds, which is rgba16f.
// DISASM: OpTypeImage %float 2D 2 0 0 1 Unknown
// DISASM-NOT: OpTypeImage %half
// DISASM-MATCH: OpFConvert %v4half %[_0-9a-zA-Z]+
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<half> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return float4(t.sample(s, float2(0.25)));
}
