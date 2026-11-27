// EXPECT: valid
// DISASM: OpUGreaterThan
// DISASM-NOT: OpSGreaterThan
//
// The large value is not among the first two enumerators: the underlying type still
// comes from the largest value of the whole enum (Apple: uint).
#include <metal_stdlib>
using namespace metal;
enum Late { lA = 1, lB = 2, lC = 0xffffffff };
kernel void enum_large_enumerator_in_third_position_is_unsigned(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Late a = lC;
	Late b = lB;
	out[i] = a > b;
}
