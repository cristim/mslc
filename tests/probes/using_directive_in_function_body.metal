// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
// DISASM-MATCH: OpReturnValue %int_33
// DISASM-NO-MATCH: OpReturnValue %int_11
// "using namespace A;" in a function body, a nested block, a helper, a loop body and
// an if's substatement holds for the rest of that block (Apple accepts each). Its names
// appear in the nearest namespace enclosing both the function and A, so inside N the
// function's own N::x is found first (33, not A's 11).
#include <metal_stdlib>
namespace A { constant int x = 11; int f() { return 12; } }
namespace N { constant int x = 33; int g() { using namespace A; return x; } }
int helper() { using namespace A; return f(); }
kernel void using_directive_in_function_body(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    using namespace metal;
    using namespace A;
    out[0] = x;
    { using namespace A; out[1] = f(); }
    for (int k = 0; k < 2; ++k) { using namespace A; out[2 + k] = f(); }
    if (i > 0) using namespace A;
    out[4] = helper() + N::g() + int(clamp(1.5f, 0.0f, 2.0f));
}
