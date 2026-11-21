// EXPECT: error texture write takes exactly a float4 or a number and a uint2 or ushort2 coordinate and a face
#include <metal_stdlib>
using namespace metal;
kernel void hit(texturecube<float, access::write> a [[texture(0)]], uint2 t [[thread_position_in_grid]]) {
  a.write(float4(1), t);
}
