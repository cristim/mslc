// EXPECT: valid
// #288 (cross-agent review of #264): a local only hides the struct tag inside the block that
// declares it; once that block ends, "S" is the struct tag again, with no namespace or using
// declaration anywhere in the file.
#include <metal_stdlib>
using namespace metal;
struct S { int y; };
kernel void k(device int* out [[buffer(0)]]) {
	{
		int S = 2;
		S = 3;
		out[1] = S;
	}
	S s;
	s.y = 7;
	out[0] = s.y;
}
