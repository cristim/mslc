// EXPECT: error is a new overload declared after a using declaration of its name
// Apple finds only the overloads declared above a using declaration through it: f(1.0f) below
// calls A::f(int). mslc does not model that, so the later overload is refused.
#include <metal_stdlib>
using namespace metal;
namespace A { float f(int x) { return 1.0f; } }
using A::f;
namespace A { float f(float x) { return 2.0f; } }
kernel void k(device float* out [[buffer(0)]])
{ out[0] = f(1.0f); }
