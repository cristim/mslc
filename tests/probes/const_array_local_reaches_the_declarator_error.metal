// EXPECT: error after a declaration, found
//
// Apple accepts "const float x[3] = {...};". mslc has no array locals; what matters is that the answer is not "needs an initialiser", since one is present.
#include <metal_stdlib>
using namespace metal;
kernel void const_array_local_reaches_the_declarator_error(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ const float x[3] = {1.0, 2.0, 3.0}; out[i] = i; }
