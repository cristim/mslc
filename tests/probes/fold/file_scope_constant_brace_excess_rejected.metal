// EXPECT: error takes one value in a brace list, found 2
//
// Apple rejects two values for a scalar constant with "excess elements in scalar initializer" (#390).
constant int k{5, 6};

kernel void file_scope_constant_brace_excess_rejected(device int *out [[buffer(0)]])
{
    out[0] = k;
}
