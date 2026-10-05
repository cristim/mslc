// EXPECT: error typedef of the struct "Missing", which is not declared
//
// Apple accepts the forward-declared struct as an incomplete type; mslc has no incomplete types.
#include <metal_stdlib>
using namespace metal;
typedef struct Missing Alias;
kernel void typedef_of_undeclared_struct_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
