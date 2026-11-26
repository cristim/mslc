// EXPECT: error cannot store to "x", a constant declared at file scope
//
// A function constant is read only.
constant int x [[function_constant(0)]];
kernel void k(device int* out [[buffer(0)]]) { x = 2; out[0] = x; }
