// EXPECT: error assigning to the reference a call returns is not lowered yet
// Apple's compiler assigns through the returned reference (metal 32023.883, macOS 26). mslc reads a returned
// reference as a value, so a store through it is refused rather than made to a copy.
#include <metal_stdlib>
using namespace metal;
struct S { int v; };
thread S& self(thread S& s) { return s; }
kernel void helper_reference_return_assigned_rejected(device int* out [[buffer(0)]]) { S s{1}; self(s) = S{9}; out[0] = s.v; }
