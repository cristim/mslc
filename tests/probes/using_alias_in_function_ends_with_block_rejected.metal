// EXPECT: error expected ;
// Apple: rejects (T is not declared after the block), so the loud refusal is the same result.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
kernel void using_alias_in_function_ends_with_block_rejected(device float *o [[buffer(0)]]) {
  {
    using T = S;
    T t = T{1.0};
    o[0] = t.a;
  }
  T u = T{2.0};
  o[1] = u.a;
}
