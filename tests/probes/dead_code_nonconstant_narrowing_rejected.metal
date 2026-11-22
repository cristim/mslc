// EXPECT: error requires a supported constant expression
//
// A non-constant int narrowed to char in dead code is rejected by Apple.
kernel void dead_code_nonconstant_narrowing_rejected(device uint *o [[buffer(0)]])
{
    return;
    int n = 300;
    char x{n};
    o[0] = x;
}
