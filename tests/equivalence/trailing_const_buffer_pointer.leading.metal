#include <metal_stdlib>
using namespace metal;
struct S { float a; };
kernel void f(const device float *p [[buffer(0)]], const device S *s [[buffer(1)]], const constant float *k [[buffer(2)]],
              device float *o [[buffer(3)]], uint i [[thread_position_in_grid]])
{ o[i] = p[i] + s[i].a + k[i]; }
