// EXPECT: error the array index of read has to be an integer
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d_array<float> t [[texture(0)]]) {
  return t.read(uint2(0, 0), 1.5);
}
