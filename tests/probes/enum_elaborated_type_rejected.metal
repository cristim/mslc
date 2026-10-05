// EXPECT: error an enum used as a type is not supported
//
// The elaborated spelling "enum Mode m" is the same use.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B };
struct S { enum Mode m; };
kernel void enum_elaborated_type_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
