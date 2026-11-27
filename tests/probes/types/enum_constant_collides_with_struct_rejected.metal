// EXPECT: error redefinition of "A"
//
// Apple accepts this, since the enumerator hides the struct name; mslc refuses to keep both meanings.
#include <metal_stdlib>
using namespace metal;
struct A { float x; };
enum One { A };
kernel void enum_constant_collides_with_struct_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
