// EXPECT: error an element of the initializer list has the enum type Mode
//
// Apple: "cannot initialize an array element of type 'const constant Mode' with an rvalue of type 'Other'".
#include <metal_stdlib>
using namespace metal;
enum Mode { MA, MB, MC };
enum Other { OA, OB };
constant Mode ka[2] = { OA, MA };
kernel void file_scope_constant_enum_array_leaf_other_enum_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = (int)ka[i & 1]; }
