// EXPECT: valid
// DISASM: OpUGreaterThan
// DISASM-NOT: OpSGreaterThan
//
// 0xaaaaaaaa does not fit an int, so the unnamed enum is unsigned int and "x > kDotted" is an unsigned comparison (Apple: 0x80000000u > kDotted is false).
#include <metal_stdlib>
using namespace metal;
enum { kNone = 0xffffffff, kDotted = 0xaaaaaaaa };
kernel void enum_large_enumerator_is_unsigned(device uint* out [[buffer(0)]], uint x [[thread_position_in_grid]])
{
	if (x > kDotted) {
		out[x] = kNone;
	}
	out[x] = x & kDotted;
}
