// EXPECT: error not lowered
// A file-scope texture stays rejected when written metal::texture2d.
#include <metal_stdlib>
using namespace metal;
metal::texture2d<float> T;
fragment float4 f() {
  return float4(0.0);
}
