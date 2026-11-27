// EXPECT: error conflicts with the using declaration of "A::k"
#include <metal_stdlib>
using namespace metal;
namespace A { constant int k = 1; }
using A::k;
template <typename T> kernel void k(device T* out [[buffer(0)]]) { out[0] = T(1); }
