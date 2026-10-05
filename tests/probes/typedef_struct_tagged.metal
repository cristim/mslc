// EXPECT: valid
// DISASM-NO-MATCH: Pair2
//
// typedef struct Tag { ... } Name makes both names the same struct, and Tag = Name is allowed.
#include <metal_stdlib>
using namespace metal;
typedef struct Pair
{
    float a;
    float b;
} Pair2;
typedef struct Same
{
    float c;
} Same;
kernel void typedef_struct_tagged(device Pair* x [[buffer(0)]], device Pair2* y [[buffer(1)]], device Same* z [[buffer(2)]], uint i [[thread_position_in_grid]])
{ x[i].a = y[i].b; z[i].c = x[i].b; }
