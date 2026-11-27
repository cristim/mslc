// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: a type brought in by a block-scope using-declaration is not dependent; Apple takes it bare.
#include <metal_stdlib>
using namespace metal;
namespace ns { struct D { float b; template <typename U> float f(U x) const { return b + float(x); } }; }
template <typename T> float g(T x) { using ns::D; D d{2.0f}; return d.f<float>(x); }
kernel void member_template_block_using_receiver_bare_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { out[i] = g(in[i]); }
