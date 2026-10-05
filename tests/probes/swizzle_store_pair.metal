// EXPECT: valid
// DISASM: %67 = OpLoad %v4float %58
// DISASM: %68 = OpVectorShuffle %v4float %67 %66 4 5 2 3
//
// A store to .xy takes the value's two lanes (second operand) and keeps the old
// zw (first operand). The ids are pinned so that swapping the operands fails.
kernel void swizzle_store_pair(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    float4 v = a[i];
    v.xy = b[i].zw;
    out[i] = v;
}
