// EXPECT: valid
// DISASM-NOT: OpUConvert
//
// A local of an enum type is assigned enumerators and compares with them.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B, C };
kernel void enum_type_as_local_and_parameter(device uint* out [[buffer(0)]])
{
	Mode m = B;
	m = C;
	out[0] = 3;
	if (m == C) {
		out[0] = 7;
	}
}
