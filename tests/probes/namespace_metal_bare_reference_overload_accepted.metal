// EXPECT: valid
// A reference parameter with no address space is still a reference, not one of the
// library's value parameter lists: mslc's own stdlibOperand needs "parameter.isReference"
// to see that, since the address space stays None here (the addressSpace guard alone
// does not catch it). Note: Apple actually rejects a bare reference parameter outright
// ("reference type must have explicit address space qualifier", confirmed against
// xcrun metal), a pre-existing mslc over-accept unrelated to this PR; this probe pins
// that this PR does not turn that pre-existing accept into a new, wrong rejection
// (mutation: deleting "parameter.isReference ||" from stdlibOperand misclassifies this
// clamp as a redefinition).
#include <metal_stdlib>
namespace metal { float clamp(float& x, float lo, float hi) { return x; } }
kernel void namespace_metal_bare_reference_overload_accepted(device float* out [[buffer(0)]])
{ out[0] = 1.0f; }
