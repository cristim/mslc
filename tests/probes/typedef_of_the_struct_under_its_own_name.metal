// EXPECT: valid
//
// typedef struct Name Name over a declared struct adds nothing and is legal.
#include <metal_stdlib>
using namespace metal;
struct Pair { float a; };
typedef struct Pair Pair;
kernel void typedef_of_the_struct_under_its_own_name(device Pair* x [[buffer(0)]], uint i [[thread_position_in_grid]])
{ x[i].a = 1.0; }
