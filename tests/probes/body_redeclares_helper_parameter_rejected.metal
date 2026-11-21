// EXPECT: error redefinition of "x"
//
// Apple: "redefinition of 'x'". A function's outermost block is the scope of its parameters.
#include <metal_stdlib>
using namespace metal;

int h(int x) { int x = 2; return x; }
kernel void body_redeclares_helper_parameter_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = h(1); }
