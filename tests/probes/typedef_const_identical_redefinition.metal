// EXPECT: valid
//
// The qualifier is part of the type a typedef names, so repeating a const typedef is the same type.
#include <metal_stdlib>
using namespace metal;
typedef const float kC;
typedef const float kC;
kernel void typedef_const_identical_redefinition(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
