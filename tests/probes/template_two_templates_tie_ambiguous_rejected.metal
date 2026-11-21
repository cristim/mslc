// EXPECT: error call to "f" is ambiguous
// Refs #341: two templates that rank alike are ambiguous (f<float>(float, float) both ways). Partial
// ordering, which C++ uses to prefer the more specialised one, is not modelled, so such a call is
// refused rather than guessed.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x, float y) { return 1.0f; }
template <typename U> float f(float x, U y) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f, 1.0f); }
