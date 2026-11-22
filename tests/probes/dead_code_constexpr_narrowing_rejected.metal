// EXPECT: error cannot be narrowed
//
// A constexpr whose value does not fit, narrowed in code after a return, is rejected by Apple.
kernel void dead_code_constexpr_narrowing_rejected(device uint *o [[buffer(0)]])
{
    return;
    constexpr int n = 300;
    char x{n};
    o[0] = x;
}
