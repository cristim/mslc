// EXPECT: error reference to "f" is ambiguous: it names A::f and B::f
// Apple calls A::f(int) (11); master silently calls B::f(float) (22), a wrong value this PR
// closes by rejecting the ambiguous call instead (overload resolution is #266).
#include <metal_stdlib>
using namespace metal;
namespace A { int f(int x) { return 11; } }
namespace B { int f(float x) { return 22; } }
using A::f;
using B::f;
kernel void k(device int* out [[buffer(0)]]) { out[0] = f(1); }
