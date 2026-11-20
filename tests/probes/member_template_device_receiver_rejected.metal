// EXPECT: error is called on an object in device
// Refs #341: Apple rejects a member function on a device object; mslc refuses a member template the same way.
#include <metal_stdlib>
using namespace metal;
struct S { float a; template <typename U> float f(U x) const { return a + float(x); } };
kernel void k(device S* o [[buffer(0)]], device float* r [[buffer(1)]])
{ r[0] = o[0].f(2); }
