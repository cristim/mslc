// EXPECT: error cannot be narrowed
//
// Apple type-checks code after a return: uchar x{300} is rejected there too.
kernel void dead_code_literal_narrowing_rejected(device uint *o [[buffer(0)]])
{
    return;
    uchar x{300};
    o[0] = x;
}
