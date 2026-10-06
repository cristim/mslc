// EXPECT: error "f" is called before it is declared
// Apple: "use of undeclared identifier 'f'".
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
float f(float y) { return y; }
