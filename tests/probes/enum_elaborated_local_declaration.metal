// EXPECT: valid
// DISASM-MATCH: = OpIEqual %bool
// DISASM-NO-MATCH: = OpFOrdEqual
//
// A plain "enum Mode m;" declaration in a function, then an assignment: the local is an
// integer, not a new enum definition.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B };
kernel void enum_elaborated_local_declaration(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	enum Mode m;
	m = B;
	out[i] = m == B;
}
