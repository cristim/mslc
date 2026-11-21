// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 1[^0-9]
// DISASM-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-MATCH: OpConstant %ulong 3[^0-9]
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 0[^0-9]
//
// An empty struct is one byte, so an array of three is 3, a struct of two of them is 2, one with
// an empty member before an int is 8 (the empty member takes a byte, the int aligns to 4), and
// one with only an empty member is 1. Apple: 1, 3, 2, 8 and 1.
#include <metal_stdlib>
using namespace metal;
struct Empty { };
struct Before { Empty e; int x; };
struct Twice { Empty a; Empty b; };
struct Only { Empty e; };
kernel void sizeof_empty_struct(device ulong *out [[buffer(0)]])
{
    out[0] = sizeof(Empty);
    out[1] = sizeof(Empty[3]);
    out[2] = sizeof(Twice);
    out[3] = sizeof(Before);
    out[4] = sizeof(Only);
}
