// EXPECT: valid
// RUN: grid=1 local=1
// Refs #385: a call through a namespace alias with no later using declaration stays accepted (1.0).
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
namespace A = N;
template <typename T> struct In { T v; float g() const { return A::h(v); } };
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) { In<int> a; a.v = s; o[0] = a.g(); }
