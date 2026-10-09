// EXPECT: error the coordinate of read has to be a uint3 or a ushort3
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture3d<float> t [[texture(0)]]) {
  return t.read(uint2(0, 0));
}
