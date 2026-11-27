// EXPECT: error texture write takes exactly a float4 or a number and a uint2 or ushort2 coordinate and a face and an array index
// Apple: no matching member function for write (a cube array write needs both).
#include <metal_stdlib>
using namespace metal;
kernel void hit(texturecube_array<float, access::write> a [[texture(0)]]) {
  a.write(float4(1), uint2(1), 1u);
}
