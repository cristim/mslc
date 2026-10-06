// EXPECT: valid
// DISASM: OpUGreaterThan
// DISASM-NOT: OpSGreaterThan
//
// The underlying type belongs to the whole enum, so kSmall - 4 is an unsigned subtraction and "> 0" is true (Apple: 1).
#include <metal_stdlib>
using namespace metal;
enum { kNone = 0xffffffff, kSmall = 3 };
kernel void enum_small_enumerator_of_an_unsigned_enum_is_unsigned(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = (kSmall - 4) > 0; }
