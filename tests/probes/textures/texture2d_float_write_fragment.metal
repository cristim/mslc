// EXPECT: valid
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float, access::write> t [[texture(0)]], float4 p [[position]]) {
  t.write(float4(1.0), uint2(p.xy));
  return float4(0.0);
}
