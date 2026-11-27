// EXPECT: error returns a reference to int, which cannot be bound to a float
// A thread int& cannot be bound to a float ([dcl.init.ref]); Apple's compiler refuses it, where reading the
// result as a value would convert it silently.
#include <metal_stdlib>
using namespace metal;
thread int& asInt(thread float& x) { return x; }
kernel void helper_reference_return_type_rejected(device int* out [[buffer(0)]]) { float a = 1.5f; out[0] = asInt(a); }
