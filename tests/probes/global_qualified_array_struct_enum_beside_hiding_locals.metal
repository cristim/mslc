// EXPECT: valid
// DISASM: OpAccessChain %_ptr_Private_float
// DISASM: %float_4 = OpConstant %float 4
// The file-scope array, struct constant and enumerator are reached through "::" while
// locals of the same names hide them: the array element is read from the Private
// variable, not from the local float "arr" (which cannot be indexed).
#include <metal_stdlib>
using namespace metal;
constant float arr[2] = {10.0, 20.0};
struct S { float x; };
constant S s0 = { .x = 4.0 };
enum E { e1 = 3 };
kernel void kern(device float* out [[buffer(0)]]) {
  float arr = 5.0; S s0; s0.x = 100.0;
  out[0] = ::arr[1] + arr; out[1] = ::s0.x + s0.x; out[2] = ::e1;
}
