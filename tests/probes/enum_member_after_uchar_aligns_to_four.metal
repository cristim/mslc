// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 8
//
// An enum member is four bytes aligned to four: after a uchar it sits at offset 4, and a following uchar at 8.
#include <metal_stdlib>
using namespace metal;
enum Mode { A, B };
struct S { uchar pad; Mode m; uchar tail; };
kernel void enum_member_after_uchar_aligns_to_four(device S* in [[buffer(0)]], device uint* out [[buffer(1)]])
{ out[0] = in[0].m; out[1] = in[0].tail; }
