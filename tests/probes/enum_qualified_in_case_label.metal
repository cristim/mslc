// EXPECT: valid
// DISASM-MATCH: OpSwitch %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 33 %
//
// "case E::v:" is a constant expression usable as a case label; the switch
// selects on the enumerator's value.
#include <metal_stdlib>
using namespace metal;
enum E { u = 22, v = 33 };
kernel void enum_qualified_in_case_label(device int *o [[buffer(0)]], device const int *in [[buffer(1)]])
{
    int r = 0;
    switch (in[0]) {
        case E::v: r = 1; break;
        default: r = 0; break;
    }
    o[0] = r;
}
