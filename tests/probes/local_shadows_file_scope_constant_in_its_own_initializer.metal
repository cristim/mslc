// EXPECT: valid
// A same-named file-scope constant is still shadowed by the local being
// declared, so `int value = 1;` below declares an int and does not read the
// float4 named `value` at file scope. The initializer is a literal, so this
// pins the declaration's own type rather than the self-reference lookup.
#include <metal_stdlib>
using namespace metal;
constant float4 value = {7, 7, 7, 7};
kernel void file_scope_constant_stays_shadowed(device int *out [[buffer(0)]]) {
  int value = 1;
  out[0] = value;
}