// EXPECT: error redefinition of "Thing"
//
// Apple: "redefinition of 'Thing'". Two struct bodies with one tag used to compile, the second silently replacing the first.
#include <metal_stdlib>
using namespace metal;
struct Thing { float a; };
struct Thing { int b; };
kernel void struct_redefinition_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
