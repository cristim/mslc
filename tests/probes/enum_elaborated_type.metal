// EXPECT: valid
// DISASM-MATCH: = OpIEqual %bool
// DISASM-NO-MATCH: = OpFOrdEqual
//
// "enum Mode m" names the same type as "Mode m", in a local and in a struct member,
// so the comparison is an integer one.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B };
struct S { enum Mode m; };
kernel void enum_elaborated_type(device S* in [[buffer(0)]], device uint* out [[buffer(1)]])
{
	enum Mode m = in[0].m;
	out[0] = m == B;
}
