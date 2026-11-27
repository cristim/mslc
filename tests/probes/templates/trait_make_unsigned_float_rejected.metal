// EXPECT: error make_unsigned of the floating-point type float has no type
// QUIRK (make_signed, make_unsigned): Apple's compiler reports "no type named 'type' in
// 'metal::_make_unsigned_impl<float>'", and the same for half and float2 (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
kernel void trait_make_unsigned_float_rejected(device uint* out [[buffer(0)]]) { out[0] = make_unsigned_t<float>(1.0f); }
