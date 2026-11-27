// EXPECT: error expected '>' to close a template parameter list
// Refs #341: the enable_if that is a non-type parameter's type (the "= 0" idiom) is not lowered; it
// is refused where the header is read, never skipped.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename enable_if<is_floating_point<T>::value, int>::type = 0> float f(T x) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
