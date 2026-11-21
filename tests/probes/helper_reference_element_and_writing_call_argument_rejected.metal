// EXPECT: error another argument reads or writes the same storage
// wr takes s.a by reference and writes it, while acc holds a copy of s.a: refused.
#include <metal_stdlib>
using namespace metal;
struct S { float a; float b; };
struct T { float v; float get() const { return v; } };
void acc(thread float &x, float v) { x += v; }
float rd(float v) { return v * 2.0f; }
float wr(thread float &y) { y = 3.0f; return 1.0f; }

kernel void k(device float *o [[buffer(0)]]) { S s; s.a = 1.0f; s.b = 7.0f; acc(s.a, wr(s.a)); o[0] = s.a; }
