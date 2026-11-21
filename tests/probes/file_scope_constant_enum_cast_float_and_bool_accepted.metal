// EXPECT: valid
// DISASM: OpConstant %int 2
// DISASM: OpConstant %int 1
//
// Apple accepts a float or bool cast to the enum: 2.5f truncates to 2, true is 1, -0.5f and 1e-40f truncate to 0. A distinct value per constant, so folding any one wrongly shows.
#include <metal_stdlib>
using namespace metal;
enum Mode { MA, MB };
constant Mode kf = (Mode)2.5f;
constant Mode kb = (Mode)true;
constant Mode kz = (Mode)-0.5f;
constant Mode ke = (Mode)1e-40f;
kernel void file_scope_constant_enum_cast_float_and_bool_accepted(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = (int)kf + (int)kb + (int)kz + (int)ke; }
