// EXPECT: valid
// DISASM: OpUGreaterThan
// DISASM-NOT: OpSGreaterThan
//
// A named enum with a value past INT_MAX has underlying type unsigned int, so two of them compare unsigned (Apple: e1a > e1b is 1).
#include <metal_stdlib>
using namespace metal;
enum E1 { e1a = 0xffffffff, e1b = 1 };
kernel void enum_named_type_with_a_large_value_is_unsigned(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	E1 a = e1a;
	E1 b = e1b;
	out[i] = a > b;
}
