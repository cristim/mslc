// EXPECT: valid
// DISASM: OpSLessThan
// DISASM-NOT: OpULessThan
//
// An enum converts to an integer and a float, takes a functional cast from an int, and promotes to int in arithmetic, so "m - 3 < 0" is a signed comparison.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B, C };
kernel void enum_value_conversions(device int* out [[buffer(0)]], device float* fout [[buffer(1)]])
{
	Mode m = Mode(2);
	Mode n = Mode(1);
	uint u = m;
	float f = n;
	out[0] = m + 1;
	out[1] = int(u) - n;
	fout[0] = f;
	if (m - 3 < 0) {
		out[2] = 1;
	}
}
