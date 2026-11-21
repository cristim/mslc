// EXPECT: error the constant "km" has the enum type Mode
//
// Apple: "cannot initialize a variable of type 'const constant Mode' with an rvalue of type 'int'". The file-scope constant folder never checked the enum, so an integer, an arithmetic result or a constexpr one all compiled.
#include <metal_stdlib>
using namespace metal;
enum Mode { MA, MB };
constexpr constant Mode km = 1 + 0;
kernel void file_scope_constant_enum_integer_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = (int)km; }
