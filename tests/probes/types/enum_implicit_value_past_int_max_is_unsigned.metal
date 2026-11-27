// EXPECT: valid
// DISASM: OpUGreaterThan
// DISASM-NOT: OpSGreaterThan
//
// B is 0x80000000, so the enum is unsigned int and "A - B > 0" is an unsigned comparison (Apple: 1).
#include <metal_stdlib>
using namespace metal;
enum One { A = 0x7fffffff, B };
kernel void enum_implicit_value_past_int_max_is_unsigned(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = (A - B) > 0; }
