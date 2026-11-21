// EXPECT: valid
// S c = ::s copies the file-scope S, though the local s is a T.
#include <metal_stdlib>
using namespace metal;
struct S { int i; }; constant S s = { .i = 5 }; struct T { int i; int j; }; kernel void kern(device int* out [[buffer(0)]]) { T s; s.i = 1; s.j = 2; S c = ::s; out[0] = c.i + s.j; }
