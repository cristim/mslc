// EXPECT: valid
//
// "(int(x) * 2)" is a parenthesised expression that starts with the
// functional cast int(x), not a C-style cast "(int" followed by "(". The
// MSL that Unity's shader translator writes for an alpha test has this form:
// if ((int(flag) * int(0xffffffffu)) != 0) { discard_fragment(); }
// The C-style casts "(int)x" and "(unsigned int)x" still parse as casts.
#include <metal_stdlib>
using namespace metal;
kernel void paren_expression_starting_with_integer_cast(device int *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    bool flag = out[i] < 0;
    int a = (int(flag) * int(0xffffffffu));
    int b = (uint(a) + 1u) > 1u ? 1 : 0;
    int c = (int)a + (int)b;
    int d = (unsigned int)c;
    if ((int(flag) * 2) != 0) {
        d = 0;
    }
    out[i] = d;
}
