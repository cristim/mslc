// EXPECT: valid
// DISASM: OpConstant %int 7
// DISASM: OpConstant %int 9
// DISASM: OpConstant %int 5
//
// Apple accepts every spelling of a cast to the enum and the enum's own enumerators and constants. The folder used to refuse the casts as "not a value that can be worked out"; the values 7, 9 and 5 read back from the module.
#include <metal_stdlib>
using namespace metal;
enum Mode { MA = 3, MB = 4 };
enum Other { OA = 9 };
constant Mode kc = (Mode)7;
constant Mode ks = static_cast<Mode>(OA);
constant Mode kf = Mode(5);
constant Mode kn = MB;
constant Mode kk = kc;
kernel void file_scope_constant_enum_cast_accepted(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = (int)kc + (int)ks + (int)kf + (int)kn + (int)kk; }
