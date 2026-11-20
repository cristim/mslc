// EXPECT: error called before it is declared
// Refs #341: a member counts as declared where the template is: Apple rejects a non-dependent name declared after it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct P { T v; float g() const { return h(); } };
float h() { return 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { P<int> p; p.v = 1; out[0] = p.g(); }
