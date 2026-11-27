// EXPECT: error parameter "p" of helper function "f" has an attribute, [[texture]], that its type
// QUIRK (helper parameter attributes): the attribute has no effect on a helper, but it must suit the type, as
// Apple's compiler requires ("type 'device float *' is not valid for attribute 'texture'").
#include <metal_stdlib>
using namespace metal;
float f(device float* p [[texture(0)]]) { return p[0]; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = f(out); }
