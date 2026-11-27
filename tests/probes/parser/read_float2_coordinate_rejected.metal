// EXPECT: error the coordinate of read has to be a uint2 or a ushort2
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.read(float2(1.0, 2.0));
}
