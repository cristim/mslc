// EXPECT: valid
// ::f names the file's f(int) and A::f(float) that a using declaration brought into the file
// scope: one overload set, and a float argument is A::f's exact match (Apple: 2).
#include <metal_stdlib>
namespace A { int f(float v) { return 2; } }
int f(int v) { return 1; }
using A::f;
kernel void kern(device int *o [[buffer(0)]]) { o[0] = ::f(1.0f) + ::f(1); }
