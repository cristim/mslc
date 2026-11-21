// EXPECT: error the texture method "write" is not lowered yet on textures that are not access::write or access::read_write
// Apple: no member function write in access::read texture2d.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read> a [[texture(0)]], uint2 t [[thread_position_in_grid]]) {
  a.write(float4(1), t);
}
