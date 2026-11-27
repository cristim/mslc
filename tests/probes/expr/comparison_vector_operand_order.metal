// EXPECT: valid
// DISASM: %49 = OpCompositeConstruct %v2int %int_1 %int_2
// DISASM: %52 = OpCompositeConstruct %v2int %int_3 %int_4
// DISASM: = OpSLessThan %v2bool %49 %52
//
// a < b compares a with b, not b with a. The two operands are built from
// distinct constants so the disassembly shows which one is on the left.
kernel void comparison_vector_operand_order(device int2 *out [[buffer(0)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = int2(int2(1, 2) < int2(3, 4));
}
