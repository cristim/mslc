// EXPECT: error reuses [[color(0)]]

#include <metal_stdlib>
using namespace metal;

struct In { float4 a [[color(0)]]; };

fragment float4 framebuffer_fetch_duplicate_index_rejected(In in [[stage_in]], float4 b [[color(0)]]) {
  return in.a + b;
}
