// EXPECT: valid
// DISASM-MATCH: OpIAdd %ulong %ulong_1[_0-9]* %[0-9]+
// DISASM-MATCH: OpIMul %ulong %ulong_1[_0-9]* %[0-9]+
// DISASM-NOT: OpConstant %ulong 16
//
// A member of the enclosing struct hides the struct tag S inside its member functions, through
// the implicit this and whatever order the members are written in: sizeof(S) is the char's 1
// (sizeof(S) + 1 is 2, sizeof (S) * 3 is 3, sizeof((S) - 1) is 4 with the minus applied to
// the int), never the struct's 16. Apple's compiler reads 2, 3, 4 and 1 for R and 2 for Late.
#include <metal_stdlib>
using namespace metal;
struct S { float4 a; };
struct R {
    char S;
    ulong plus() const { return sizeof(S) + 1; }
    ulong times() const { return sizeof (S) * 3; }
    ulong paren() const { return sizeof((S) - 1); }
    ulong bare() const { return sizeof S; }
};
struct Late {
    ulong plus() const { return sizeof(S) + 1; }
    char S;
};
kernel void sizeof_struct_tag_hidden_by_member_field(device ulong *out [[buffer(0)]])
{
    R r;
    r.S = 1;
    Late late;
    late.S = 1;
    out[0] = r.plus();
    out[1] = r.times();
    out[2] = r.paren();
    out[3] = r.bare();
    out[4] = late.plus();
}
