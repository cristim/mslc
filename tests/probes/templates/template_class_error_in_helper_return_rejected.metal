// EXPECT: error in the class template "P" (declared at line 6), instantiated as "P<float>"
// Refs #341: the parser backtracks over a helper's return type; an instance that failed in a member is an error there, and
// not a struct that is half declared and found again by the second reading.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct P { T v; float g() const { return 1.0f +; } };
P<float> make() { P<float> p; return p; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = make().v; }
