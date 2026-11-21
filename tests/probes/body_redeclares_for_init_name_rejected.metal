// EXPECT: error redefinition of "x"
//
// Apple: "redefinition of 'x'". A for body's outermost block is the scope of the init declaration.
#include <metal_stdlib>
using namespace metal;

kernel void body_redeclares_for_init_name_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ for (int x = 0; x < 1; x++) { int x = 2; o[i] = x; } }
