// EXPECT: error the function template "f" is declared and never defined
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x);
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1); }
