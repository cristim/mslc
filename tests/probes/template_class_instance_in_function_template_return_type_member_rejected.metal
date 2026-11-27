// EXPECT: error ".v" is not a swizzle
// Refs #341: a return type Box<T> is read while T is still being deduced; the instance the call chooses has
// the real one, but a member taken from the call is read as a swizzle and refused, as for a template returning
// T (template_struct_result_member_rejected); assign the result to a variable first.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> Box<T> make(T x) { Box<T> b; b.v = x; return b; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = make(3.0f).v; }
