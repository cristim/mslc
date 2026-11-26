// EXPECT: error the pointer "p" is used as a value (a copy, a comparison or an arithmetic result kept in a local); mslc does not lower pointer values yet; index it, as p[i]
// A member taken straight off a pointer arithmetic result, (p + i)->x, is a
// chain rooted on an arithmetic value, which is not lowered; p[i].x is the
// same element spelled the way mslc lowers it.
#include <metal_stdlib>
using namespace metal;
struct S { float x; };
float f(device float* p, uint i) { return (p + i)->x; }
kernel void helper_pointer_arith_member_rejected(device float* out [[buffer(0)]],
                                                 device float* in [[buffer(1)]],
                                                 uint i [[thread_position_in_grid]])
{ out[i] = f(in, i); }
