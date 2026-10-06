#include <metal_stdlib>
using namespace metal;
struct S { float a; };
kernel void f(constant S const &c [[buffer(0)]], device S const &d [[buffer(1)]], device float *o [[buffer(2)]])
{ o[0] = c.a + d.a; }
