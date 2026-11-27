// EXPECT: error redefinition of "n"
//
// Apple: "redefinition of 'n'".
#include <metal_stdlib>
using namespace metal;

kernel void body_redeclares_entry_parameter_rejected(device int* o [[buffer(0)]], constant int& n [[buffer(1)]], uint i [[thread_position_in_grid]])
{ int n = 2; o[i] = n; }
