// EXPECT: valid
// DISASM: OpConstant %int 2147483520
//
// The largest float below 2^31 fits the underlying type and folds.
#include <metal_stdlib>
using namespace metal;
enum Mode { MA, MB };
constant Mode k = (Mode)2147483520.0f;
kernel void file_scope_constant_enum_cast_float_inside_range_accepted(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = (int)k; }
