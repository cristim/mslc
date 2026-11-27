// EXPECT: error a bare enum declaration without enumerators
//
// Apple rejects an opaque enum without a fixed underlying type.
#include <metal_stdlib>
using namespace metal;
enum Mode;
kernel void enum_without_body_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
