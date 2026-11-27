// EXPECT: valid
// REFLECT: "mag_filter": "Nearest"
// DISASM: OpSampledImage
#include <metal_stdlib>
using namespace metal;
kernel void f(texture2d<float> t [[texture(0)]], device float4* out [[buffer(0)]], uint x [[thread_position_in_grid]]) {
  switch (x) {
    default: {
      constexpr sampler s(coord::normalized, filter::nearest);
      out[x] = t.sample(s, float2(0.25f));
      break;
    }
  }
}
