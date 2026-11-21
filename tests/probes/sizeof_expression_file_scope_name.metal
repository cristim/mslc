// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 1[^0-9]
//
// ::x is the file-scope x whatever local hides it, in an operand that is more than the bare name
// as in the bare one: a half x hides the file-scope float x (4) in a block and in a for
// initialiser, and ::x + 1.0h, -::x, ::x * ::x, a conditional, a comma, an element of ::a and a
// member of ::s are all that float. Apple: 4 for each; the half it hides is 2.
#include <metal_stdlib>
using namespace metal;
struct Pair { float a; int b; };
constant float x = 1.0f;
constant float a[2] = { 1.0f, 2.0f };
constant Pair s = { .a = 1.0f, .b = 2 };
kernel void sizeof_expression_file_scope_name(device ulong *out [[buffer(0)]])
{
    half x = 0;
    half a[2] = { 0.0h, 0.0h };
    half s = 0;
    out[0] = sizeof(::x + 1.0h);
    out[1] = sizeof(-::x);
    out[2] = sizeof(::x * ::x);
    out[3] = sizeof(::a[0] + ::a[0]);
    out[4] = sizeof(true ? ::x : ::x);
    out[5] = sizeof((0, ::x));
    out[6] = sizeof(::s.a + ::s.a);
    for (half x = 0; x < 1; x++) {
        out[7] = sizeof(::x * ::x);
    }
}
