// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 4[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 1[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 2[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 8[^0-9]
//
// An operand narrower than an int is promoted to one before an operator takes it, a bool
// included: bool + bool, char + char, uchar + uchar, short + ushort, ~s, -b and s << c are all
// ints (4), where a bool by itself is 1 and a comparison is a bool. Apple gives 4 for each.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_promotes_to_int(device ulong *out [[buffer(0)]])
{
    bool b = true;
    char c = 0;
    uchar uc = 0;
    short s = 0;
    ushort us = 0;
    out[0] = sizeof(b + b);
    out[1] = sizeof(c + c);
    out[2] = sizeof(uc + uc);
    out[3] = sizeof(s + us);
    out[4] = sizeof(~s);
    out[5] = sizeof(-b);
    out[6] = sizeof(s << c);
}
