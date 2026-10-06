// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 20
//
// A named and a typedef'd enum as struct members take the four bytes of an int at the next aligned offset, as they do in Apple's compiler.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B, C };
typedef enum { P = 0, Q = 5 } Kind;
typedef struct { float4 c; Mode mode; Kind kind; } Info;
kernel void enum_type_as_buffer_field(device Info* in [[buffer(0)]], device uint* out [[buffer(1)]])
{
	out[0] = 0;
	if (in[0].mode == B) {
		out[0] = 1;
	}
	out[1] = in[0].kind;
}
