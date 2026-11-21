// EXPECT: valid
// DISASM: OpConstant %ulong 2
// DISASM-MATCH: OpISub %ulong %ulong_2[_0-9]* %[0-9]+
// DISASM-MATCH: OpIMul %ulong %ulong_2[_0-9]* %[0-9]+
// DISASM-NOT: OpConstant %ulong 16
//
// A local named S hides the struct tag S, so sizeof(S) is the size of the half (2) and
// "(S) - 1" is that half minus one, not a cast to the struct (16 bytes). Apple's compiler reads
// 1, 200, 2 and 2 for the four constexprs below (the subtraction and the product are built
// from the half's 2, never from the struct's 16).
#include <metal_stdlib>
using namespace metal;
struct S { float4 a; };
kernel void sizeof_struct_tag_hidden_by_local(device ulong *out [[buffer(0)]])
{
    half S = 1;
    constexpr ulong minus = sizeof(S) - 1;
    constexpr ulong times = sizeof(S) * 100;
    constexpr ulong parenthesised = sizeof((S) - 1);
    constexpr ulong sum = sizeof(S + 1);
    out[0] = minus;
    out[1] = times;
    out[2] = parenthesised;
    out[3] = sum;
}
