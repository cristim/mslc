// EXPECT: valid
//
// Apple accepts an array of the enum's own enumerators and casts.
#include <metal_stdlib>
using namespace metal;
enum Mode { MA, MB, MC };
constant Mode ka[3] = { MC, (Mode)1, static_cast<Mode>(0) };
kernel void file_scope_constant_enum_array_own_leaves_accepted(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = (int)ka[i % 3]; }
