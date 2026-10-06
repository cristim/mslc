#include <metal_stdlib>
using namespace metal;
struct S { float a; };
kernel void f(device float const *p [[buffer(0)]], device S const *s [[buffer(1)]], constant float const *k [[buffer(2)]],
              device float * const o [[buffer(3)]], uint i [[thread_position_in_grid]])
{ o[i] = p[i] + s[i].a + k[i]; }
