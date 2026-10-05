// EXPECT: error redefinition of "Thing"
//
// typedef struct Thing {...} Thing makes Thing a typedef, which a function of that name collides with. Apple: "redefinition of 'Thing' as different kind of symbol".
#include <metal_stdlib>
using namespace metal;
typedef struct Thing { float a; } Thing;
kernel void Thing(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
