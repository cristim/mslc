// EXPECT: valid
//
// Apple accepts: a nested block may shadow a parameter or a for-init name; only the outermost block shares their scope.
#include <metal_stdlib>
using namespace metal;

int h(int x) { { int x = 2; return x; } }
kernel void nested_block_shadows_parameter_and_for_init_accepted(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ for (int x = 0; x < 1; x++) { { int x = 2; o[i] = x + h(1); } } for (int y = 0; y < 1; y++) { for (int y = 0; y < 1; y++) { o[i] += y; } } }
