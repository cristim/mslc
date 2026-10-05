// EXPECT: error redefinition of "Thing"
//
// Apple: "typedef redefinition with different types ('int' vs 'Thing')".
#include <metal_stdlib>
using namespace metal;
struct Thing { float a; };
typedef int Thing;
kernel void typedef_collides_with_struct_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
