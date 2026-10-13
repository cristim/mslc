// EXPECT: valid
// DISASM-NO-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
// DISASM-NO-MATCH: OpStore %[_0-9a-zA-Z]+ %int_44[_0-9]* Aligned
// A local named like a name a using directive or declaration brings in hides it (Apple
// accepts): x is the local, not A::x (11) or the file's x (44), and "S = 3;" assigns the
// local S rather than declaring an A::S. Before #264 these were "ambiguous" or a parse error (#288).
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; struct S { int y; }; }
constant int x = 44;
using namespace A;
kernel void local_hides_using_introduced_name(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    int x = int(i);
    out[0] = x;
    int S = int(i) + 1;
    S = 3;
    out[1] = S;
}
