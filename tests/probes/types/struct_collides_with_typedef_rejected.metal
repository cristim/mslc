// EXPECT: error redefinition of "Thing"
//
// Apple: "definition of type 'Thing' conflicts with typedef of the same name".
#include <metal_stdlib>
using namespace metal;
typedef int Thing;
struct Thing { float a; };
kernel void struct_collides_with_typedef_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
