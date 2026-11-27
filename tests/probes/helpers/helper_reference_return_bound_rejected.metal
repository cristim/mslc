// EXPECT: error binding it to a value is not lowered yet
// Apple's compiler binds a reference to the reference a call returns (metal 32023.883, macOS 26); mslc reads the
// result as a value, which a reference cannot be bound to.
#include <metal_stdlib>
using namespace metal;
thread int& self(thread int& x) { return x; }
kernel void helper_reference_return_bound_rejected(device int* out [[buffer(0)]]) { int a = 1; thread int& r = self(a); r = 5; out[0] = a; }
