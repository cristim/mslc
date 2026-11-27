// EXPECT: error returns a reference without an address space, which MSL requires
// Apple's compiler reports "reference type must have explicit address space qualifier" (metal 32023.883, macOS 26).
#include <metal_stdlib>
using namespace metal;
int& self(thread int& x) { return x; }
kernel void helper_reference_return_no_address_space_rejected(device int* out [[buffer(0)]]) { int a = 1; out[0] = self(a); }
