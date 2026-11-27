// EXPECT: error write ".template f<...>(...)"
// Refs #386: a dependent index makes the element dependent; Apple rejects it bare (use 'template' keyword).
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> float g(T x) { B loc[2] = {B{3.0f}, B{4.0f}}; return loc[x].f<float>(1.0f); }
kernel void member_template_element_dependent_index_receiver_bare_rejected(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { out[i] = g(1u); }
