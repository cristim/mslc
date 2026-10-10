// EXPECT: error texture2d<E> is not lowered yet
// An enum is not a sampled type, even though it is stored as an integer.
#include <metal_stdlib>
using namespace metal;
enum E { A = 0x80000000u };
fragment float4 f(texture2d<E, access::write> t [[texture(0)]]) {
  t.write(1u, uint2(0));
  return float4(0.0);
}
