// EXPECT: error already declared
// Apple: accepts (the inner alias hides the outer); mslc refuses a function alias whose name is taken (#251)
#include <metal_stdlib>
using namespace metal;
kernel void using_alias_in_function_hides_outer_alias_rejected(device float *o [[buffer(0)]]) {
  using T = int;
  {
    using T = float;
    T x = 1.5;
    o[0] = x;
  }
}
