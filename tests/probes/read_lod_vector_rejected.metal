// EXPECT: error the lod of read has to be a number
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.read(uint2(0), uint2(1));
}
