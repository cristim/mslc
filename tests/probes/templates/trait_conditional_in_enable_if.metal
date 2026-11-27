// EXPECT: valid
// RUN: grid=1 local=1
// A conditional whose condition depends on T, inside an enable_if in a function template's return type: read
// before T is deduced it holds for now, and the instance decides. f(2u) picks the template, f(2.0f) the float
// overload. On an RTX 3060 out is 2 7.
#include <metal_stdlib>
using namespace metal;
template <typename T> enable_if_t<is_same_v<conditional_t<is_scalar_v<T>, T, int>, uint>, T> f(T x) { return x; }
uint f(float) { return 7u; }
kernel void trait_conditional_in_enable_if(device uint* out [[buffer(0)]]) {
    out[0] = f(2u);
    out[1] = f(2.0f);
}
