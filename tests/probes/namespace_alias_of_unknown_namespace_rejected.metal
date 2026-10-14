// EXPECT: error "Q" is not a namespace the file declares
// Apple: "expected namespace name" (Q is a variable).
#include <metal_stdlib>
using namespace metal;
constant int Q = 1;
namespace B = Q;
kernel void namespace_alias_of_unknown_namespace_rejected(device int* out [[buffer(0)]])
{ out[0] = 1; }
