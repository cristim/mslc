#include <metal_stdlib>
using namespace metal;
struct S { float a; };
kernel void f(const constant S &c [[buffer(0)]], const device S &d [[buffer(1)]], device float *o [[buffer(2)]])
{ o[0] = c.a + d.a; }
