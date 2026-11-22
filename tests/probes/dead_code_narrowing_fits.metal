// EXPECT: valid
//
// A narrowing that fits is accepted in dead code, as in live code, and the dead declaration
// emits nothing.
kernel void dead_code_narrowing_fits(device uint *o [[buffer(0)]])
{
    o[0] = 1;
    return;
    uchar x{200};
    constexpr int n = 100;
    char y{n};
    o[0] = x + y;
}
