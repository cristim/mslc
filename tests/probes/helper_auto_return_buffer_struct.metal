// EXPECT: valid
//
// An auto helper returning a struct read from a buffer, by its element and by dereferencing a pointer, and
// returning a local of the same struct from another return: the buffer holds the laid-out form, and each return
// gives the value form the function returns.
#include <metal_stdlib>
using namespace metal;
struct S { float3 a; float b; };
auto element(device const S* in, uint i) { return in[i]; }
auto first(device const S* in) { return *in; }
auto either(device const S* in, bool c) { S t = in[0]; if (c) { return t; } return in[1]; }
kernel void helper_auto_return_buffer_struct(device S* out [[buffer(0)]], device const S* in [[buffer(1)]],
    uint i [[thread_position_in_grid]]) {
  out[i] = element(in, i);
  out[i].b += first(in).b + either(in, i > 2).a.x;
}
