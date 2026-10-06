// EXPECT: valid
// DISASM: OpUGreaterThan
// DISASM-NOT: OpSGreaterThan
// DISASM: OpShiftRightLogical
//
// B is the implicit 0x80000000, so B alone is unsigned: "B > 0" is an unsigned compare
// and "B >> 31" is a logical shift (Apple: 1).
#include <metal_stdlib>
using namespace metal;
enum Implicit { iA = 0x7fffffff, iB };
kernel void enum_implicit_value_alone_is_unsigned(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	out[i] = uint(iB > 0);
	out[i + 1] = uint(iB >> 31);
}
