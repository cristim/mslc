// EXPECT: valid
// DISASM: OpUGreaterThan
// DISASM-NOT: OpSGreaterThan
//
// "typedef enum Tag {...} Name": the unsigned underlying type is found through either
// spelling of the type.
#include <metal_stdlib>
using namespace metal;
typedef enum TagLarge { tA = 0xffffffff, tB = 1 } NameLarge;
kernel void enum_typedef_tag_differs_from_name_is_unsigned(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	NameLarge a = tA;
	enum TagLarge b = tB;
	out[i] = a > b;
}
