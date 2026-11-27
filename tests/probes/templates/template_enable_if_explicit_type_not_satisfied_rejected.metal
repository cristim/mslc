// EXPECT: error no matching function for call to "f"
// Refs #341: f<int> names T and leaves E to its default, which is not satisfied for int.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename E = typename enable_if<is_floating_point<T>::value>::type> float f(T x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f<int>(1); }
