// EXPECT: error write ".template f<...>(...)"
// Refs #386: a call argument that indexes by a dependent value is dependent; Apple rejects it bare (use 'template' keyword).
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
B mk(float v) { return B{v}; }
template <typename T> float g(T x) { B loc[2] = {B{3.0f}, B{4.0f}}; return mk(loc[x].b).f<float>(1.0f); }
kernel void member_template_call_argument_dependent_element_receiver_bare_rejected(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { out[i] = g(1u); }
