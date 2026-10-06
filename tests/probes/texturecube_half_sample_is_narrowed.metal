// EXPECT: valid
// A half cube is the same float image as a float one, sampled as float and narrowed:
// Vulkan has no 16-bit sampled type (indium src/iridium/air.cpp:1187-1190).
// DISASM: OpTypeImage %float Cube 2 0 0 1 Unknown
// DISASM-MATCH: OpFConvert %v4half %[_0-9a-zA-Z]+
#include <metal_stdlib>
using namespace metal;
fragment half4 f(texturecube<half> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float3(0.25, 0.5, 0.75));
}
