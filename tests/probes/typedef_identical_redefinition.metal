// EXPECT: valid
//
// Repeating a typedef with the same type is not an error, in C++ and in Apple's compiler.
#include <metal_stdlib>
using namespace metal;
typedef float scalar_t;
typedef float scalar_t;
typedef float float_again;
kernel void typedef_identical_redefinition(device scalar_t* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0; }
