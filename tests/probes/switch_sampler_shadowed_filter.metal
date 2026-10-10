// EXPECT: valid
// REFLECT: "mag_filter": "Nearest"
// REFLECT: "mag_filter": "Linear"
// REFLECT: "descriptor": { "set": 0, "binding": 2 }, "embedded_sampler": 0
// REFLECT: "descriptor": { "set": 0, "binding": 3 }, "embedded_sampler": 1
// DISASM: OpDecorate %35 Binding 2
// DISASM: OpDecorate %38 Binding 3
// DISASM: %35 = OpVariable %_ptr_UniformConstant_33 UniformConstant
// DISASM: %38 = OpVariable %_ptr_UniformConstant_33 UniformConstant
// DISASM-MATCH: %68 = OpLoad %33 %35([^0-9]|$)
// DISASM-MATCH: %70 = OpSampledImage %69 %67 %68([^0-9]|$)
// DISASM-MATCH: %88 = OpLoad %33 %38([^0-9]|$)
// DISASM-MATCH: %89 = OpSampledImage %69 %87 %88([^0-9]|$)
#include <metal_stdlib>
using namespace metal;
kernel void f(texture2d<float> t [[texture(0)]], device float4* out [[buffer(0)]], uint x [[thread_position_in_grid]]) {
  constexpr sampler s(coord::normalized, filter::nearest);
  out[x + 32] = t.sample(s, float2(0.25f));
  switch (x) {
    default: {
      constexpr sampler s(coord::normalized, filter::linear);
      out[x] = t.sample(s, float2(0.25f));
      break;
    }
  }
}
