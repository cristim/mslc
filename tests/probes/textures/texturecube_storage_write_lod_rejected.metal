// EXPECT: error mip arguments are unsupported on access::write textures
// Apple accepts write(value, coord, face, lod); the lod would be dropped silently, so it is refused.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texturecube<float, access::write> a [[texture(0)]], uint3 t [[thread_position_in_grid]]) {
  a.write(float4(1), t.xy, t.z, 1u);
}
