// EXPECT: error has an attribute, [[base_vertex]], that its type
// QUIRK (helper parameter attributes): [[base_vertex]] needs a uint or ushort scalar by value on a helper too;
// Apple's compiler reports "type 'uint &' ... is not valid for attribute 'base_vertex'", and the same for int
// and float.
#include <metal_stdlib>
using namespace metal;
float f(thread uint& b [[base_vertex]]) { return float(b); }
vertex float4 v() { uint x = 1; return float4(f(x)); }
