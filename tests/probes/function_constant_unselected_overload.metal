// EXPECT: valid
// DISASM-NOT: Float16
// REFLECT: { "name": "i", "type": "int", "index": 1,
// REFLECT-NOT: "name": "h"
//
// A function constant that only an overload the kernel does not call reads is not listed, and not
// declared, so its half type asks for no Float16 capability.
#include <metal_stdlib>
using namespace metal;
constant half h [[function_constant(0)]];
constant int i [[function_constant(1)]];
int pick(int x) { return x + i; }
float pick(float x) { return x + float(h); }
kernel void k(device int* out [[buffer(0)]]) { out[0] = pick(1); }
