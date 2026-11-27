// EXPECT: error already declared
// Apple: accepts, and the alias hides the struct; mslc refuses a function alias whose name is taken (#251)
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
kernel void using_alias_in_function_hides_declaration_rejected(device float *o [[buffer(0)]]) {
  using S = float;
  S x = 1.0;
  o[0] = x;
}
