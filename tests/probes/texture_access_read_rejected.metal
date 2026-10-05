// EXPECT: error texture2d access::read is not lowered yet
// Apple accepts it; the reflection's texture_access would have to say Read and the descriptor would change.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float, access::read> t [[texture(0)]]) {
  return float4(0.0);
}
