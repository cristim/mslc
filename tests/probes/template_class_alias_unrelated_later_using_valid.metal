// EXPECT: valid
// RUN: grid=1 local=1
// Refs #385: a later using in P, which the alias does not name, is accepted (1.0).
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
namespace M { float h(int x) { return 7.0f; } }
namespace P { float q(float x) { return 2.0f; } }
namespace A = N;
template <typename T> struct In { T v; float g() const { return A::h(v); } };
namespace P { using M::h; }
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) { In<int> a; a.v = s; o[0] = a.g(); }
