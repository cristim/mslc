// EXPECT: valid
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1_998pn4
// DISASM-MATCH: = OpFMul %half %[0-9]+ %[0-9]+
// DISASM-NOT: %float
//
// (h + 0.1h) * h2 rounds to half after the sum and again after the product.
kernel void half_literal_rounds_at_each_step(device half *out [[buffer(0)]],
                                             device const half *in [[buffer(1)]],
                                             device const half *in2 [[buffer(2)]],
                                             uint i [[thread_position_in_grid]])
{
    out[i] = (in[i] + 0.1h) * in2[i];
}
