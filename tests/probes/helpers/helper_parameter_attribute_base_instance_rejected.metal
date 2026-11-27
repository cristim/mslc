// EXPECT: error has an attribute, [[base_instance]], that its type uint3 is not valid for
// QUIRK (helper parameter attributes): [[base_instance]] and [[base_vertex]] need a uint or ushort scalar on a
// helper too, as Apple's compiler reports ("type 'uint3' ... is not valid for attribute 'base_instance'").
#include <metal_stdlib>
using namespace metal;
float f(uint3 b [[base_instance]]) { return float(b.x); }
vertex float4 v() { return float4(f(uint3(1))); }
