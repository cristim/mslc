// EXPECT: valid
//
// A typedef of a struct that is already declared names it again.
#include <metal_stdlib>
using namespace metal;
struct Pair
{
    float a;
};
typedef struct Pair Alias;
typedef Pair Alias;
kernel void typedef_of_a_declared_struct(device Alias* x [[buffer(0)]], uint i [[thread_position_in_grid]])
{ x[i].a = 1.0; }
