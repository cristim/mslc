// EXPECT: valid
// REFLECT: "name": "fc"
//
// An auto helper first called inside an initialiser element that is emitted for its diagnostics and then lifted
// out (brace elision into a struct member): the helper is kept, and the function constant it reads is listed.
#include <metal_stdlib>
using namespace metal;
constant float fc [[function_constant(3)]];
struct In { float a; };
struct O { In i; float b; };
auto f(float x) { In r; r.a = x * fc; return r; }
kernel void helper_auto_return_function_constant_lifted(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  O o = { f(float(i)), 2 }; out[i] = o.i.a + o.b;
}
