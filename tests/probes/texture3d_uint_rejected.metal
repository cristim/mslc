// EXPECT: error texture3d<uint> is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture3d<uint> t [[texture(0)]]) {
  return float4(0.0);
}
