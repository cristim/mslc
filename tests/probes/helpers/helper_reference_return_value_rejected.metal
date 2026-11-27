// EXPECT: error returns a reference, which has to be bound to a variable, an element or a member
// A reference that is not const cannot be bound to the value x + 1 ([dcl.init.ref]); Apple's compiler refuses it.
#include <metal_stdlib>
using namespace metal;
thread int& next(thread int& x) { return x + 1; }
kernel void helper_reference_return_value_rejected(device int* out [[buffer(0)]]) { int a = 1; out[0] = next(a); }
