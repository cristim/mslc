// EXPECT: error unknown type name "Nope"
// Apple: unknown type name 'Nope'
#include <metal_stdlib>
using namespace metal;
using T = Nope;
kernel void using_alias_of_undeclared_type_rejected(device float *o [[buffer(0)]]) { o[0] = 1; }
