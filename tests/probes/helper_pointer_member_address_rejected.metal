// EXPECT: error argument 1 of the call to "g" is & of something that is not a whole buffer or one of its elements; &b[i] is the form a pointer argument takes, and &b[i].m is not lowered yet
// A pointer to a member of an element, &s[i].x, has no element stride of its
// own to walk later, so it is refused this slice.
#include <metal_stdlib>
using namespace metal;
struct S { float x; float y; };
void g(device float* p, uint i, float v) { p[i] = v; }
kernel void helper_pointer_member_address_rejected(device float* out [[buffer(0)]],
                                                   device S* s [[buffer(1)]],
                                                   uint i [[thread_position_in_grid]])
{ g(&s[i].x, i, 1.0f); }
