// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_22[_0-9]* Aligned
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_13[_0-9]* Aligned
// DISASM-NO-MATCH: OpStore %[_0-9a-zA-Z]+ %int_44[_0-9]* Aligned
// "using A::x;" declares x in the block: it hides the file's x (44) and the outer local
// x, an inner local or an inner using declaration hides it in turn, and it ends with the
// block. A type and an enumerator can be brought in the same way, and an inner local S
// hides a using-declared struct S, so "S = 3;" is an assignment. Apple accepts all.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; struct S { int y; }; enum E { v = 13 }; }
namespace B { constant int x = 22; }
constant int x = 44;
kernel void using_declaration_in_function_body(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    int y = int(i);
    {
        int x = y + 100;
        {
            using A::x;
            using A::x;
            out[0] = x;
            { using B::x; out[1] = x; }
            { int x = y; out[2] = x; }
        }
        out[6] = x;
    }
    {
        using A::S;
        { int S = y; S = 3; out[7] = S; }
    }
    {
        using A::S;
        using A::E;
        using A::v;
        S s;
        s.y = y;
        E e = v;
        out[3] = v;
        out[5] = int(e);
        out[4] = s.y;
    }
}
