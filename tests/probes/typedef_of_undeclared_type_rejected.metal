// EXPECT: error typedef of the type "Missing", which is not declared
//
// Apple: "unknown type name 'Missing'".
#include <metal_stdlib>
using namespace metal;
typedef Missing Alias;
kernel void typedef_of_undeclared_type_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
