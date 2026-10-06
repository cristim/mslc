// EXPECT: error a call to "g" returns void and has no value
// Apple: "cannot initialize return object of type 'float' with an rvalue of type 'void'".
#include <metal_stdlib>
using namespace metal;
void g(float x) { }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ float v = g(1.0f); out[i] = v; }
