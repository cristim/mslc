// EXPECT: error no matching function for call to "f"
// Refs #341: the default drops the template for an int, and nothing else is called f, so the call
// has no function (Apple: no matching function). It is not silently instantiated for T = int.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename E = typename enable_if<is_floating_point<T>::value>::type> float f(T x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1); }
