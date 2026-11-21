// EXPECT: valid
// DISASM: %int_111
// DISASM-NOT: %int_222
// A constant enum binds the reference to its own enum, and not the one to int (the type differs).
#include <metal_stdlib>
using namespace metal;
enum E { X, Y };
constant E KE = Y;
int f(const constant E &x) { return 111; }
int f(const constant int &x) { return 222; }
kernel void k(device int *o [[buffer(0)]]) { o[0] = f(KE); }
