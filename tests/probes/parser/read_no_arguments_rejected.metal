// EXPECT: error read takes a coordinate and optionally a lod
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.read();
}
