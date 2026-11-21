// EXPECT: valid
// DISASM-MATCH: OpIAdd %ulong %ulong_2[_0-9]* %[0-9]+
// DISASM: OpConstant %ulong 2
// DISASM-NOT: OpConstant %ulong 16
//
// A file-scope constant named S hides the struct tag S as well: sizeof(S) + 1 is 3 and
// sizeof((S) + 1) is 2 (the half), not the struct's 16. Apple's compiler agrees.
#include <metal_stdlib>
using namespace metal;
struct S { float4 a; };
constant half S = 1;
kernel void sizeof_struct_tag_hidden_by_global(device ulong *out [[buffer(0)]])
{
    constexpr ulong plus = sizeof(S) + 1;
    constexpr ulong parenthesised = sizeof((S) + 1);
    out[0] = plus;
    out[1] = parenthesised;
}
