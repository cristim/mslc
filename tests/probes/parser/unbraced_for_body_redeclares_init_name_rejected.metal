// EXPECT: error redefinition of "x"
//
// Apple: "redefinition of 'x'". An unbraced for body that is a declaration shares the init declaration's scope too.
#include <metal_stdlib>
using namespace metal;
kernel void unbraced_for_body_redeclares_init_name_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ for (int x = 0; x < 1; x++) int x = 2; o[i] = 0; }
