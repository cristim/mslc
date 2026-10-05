// EXPECT: valid
// DISASM: OpConstant %int 7
//
// A struct field may have the name of an enumerator: it is reached by member access, not by name lookup.
#include <metal_stdlib>
using namespace metal;
enum { Alpha = 7 };
struct S { uint Alpha; };
kernel void enum_constant_named_like_a_field(device S* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i].Alpha = Alpha; }
