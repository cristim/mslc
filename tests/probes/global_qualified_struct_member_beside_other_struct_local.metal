// EXPECT: valid
// ::s.i names S::i of the file-scope s; the local s is a T with no i.
#include <metal_stdlib>
using namespace metal;
struct S { int i; }; constant S s = { .i = 5 }; struct T { int j; }; kernel void kern(device int* out [[buffer(0)]]) { T s; s.j = 1; out[0] = ::s.i + s.j; }
