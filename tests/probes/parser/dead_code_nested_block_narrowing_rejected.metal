// EXPECT: error cannot be narrowed
//
// A narrowing in a block after a return is rejected by Apple, as in live code.
kernel void dead_code_nested_block_narrowing_rejected(device uint *o [[buffer(0)]])
{
    return;
    {
        uchar x{300};
        o[0] = x;
    }
}
