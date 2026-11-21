// EXPECT: error the constant "km" has the enum type Mode
//
// Apple: "cannot initialize a variable of type 'const constant Mode' with an rvalue of type 'Other'".
#include <metal_stdlib>
using namespace metal;
enum Mode { MA, MB };
enum Other { OA, OB };
constant Mode km = OA;
kernel void file_scope_constant_enum_other_enum_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = (int)km; }
