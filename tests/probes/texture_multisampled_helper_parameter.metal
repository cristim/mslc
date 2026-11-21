// EXPECT: valid
// A multisampled texture is its own helper parameter type.
// DISASM: OpFunctionCall
#include <metal_stdlib>
using namespace metal;
float4 firstSample(texture2d_ms<float> t, uint2 p) { return t.read(p, 0); }
fragment float4 hit(texture2d_ms<float> a [[texture(0)]], float4 pos [[position]]) { return firstSample(a, uint2(pos.xy)); }
