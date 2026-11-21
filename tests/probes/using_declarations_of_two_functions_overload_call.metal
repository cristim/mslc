// EXPECT: valid
// DISASM: OpFunctionCall %int
// Two using declarations bring A::f(int) and B::f(float) in under one name: they are one
// overload set, and f(1) is A::f (Apple calls it, and returns 11). Master read the two names
// as an ambiguous reference.
#include <metal_stdlib>
using namespace metal;
namespace A { int f(int x) { return 11; } }
namespace B { int f(float x) { return 22; } }
using A::f;
using B::f;
kernel void k(device int* out [[buffer(0)]]) { out[0] = f(1) + f(1.0f); }
