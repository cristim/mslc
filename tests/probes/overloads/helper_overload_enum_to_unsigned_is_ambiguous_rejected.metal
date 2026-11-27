// EXPECT: error call to "f" is ambiguous
// An enum promotes to int only: to uint and to float it is a conversion each (Apple).
#include <metal_stdlib>
using namespace metal;
enum E { ea, eb };
float f(uint x) { return 1.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(ea); }
