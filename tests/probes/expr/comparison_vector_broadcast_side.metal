// EXPECT: valid
// DISASM: %57 = OpCompositeConstruct %v2int %int_7 %int_7
// DISASM: = OpSLessThan %v2bool %57 %56
// DISASM: %70 = OpCompositeConstruct %v2int %int_9 %int_9
// DISASM: = OpSLessThan %v2bool %68 %70
//
// The scalar is broadcast on the side it is written: 7 < v has the broadcast
// 7 on the left, v < 9 has the broadcast 9 on the right.
kernel void comparison_vector_broadcast_side(device int2 *out [[buffer(0)]],
                                             constant int2 *a [[buffer(1)]],
                                             uint i [[thread_position_in_grid]])
{
    out[i] = int2(7 < a[i]) + int2(a[i] < 9);
}
