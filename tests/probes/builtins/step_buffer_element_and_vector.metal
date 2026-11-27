// EXPECT: valid
// DISASM-MATCH: = OpFAdd %v4float %[0-9]+ %[0-9]+
// DISASM-MATCH: = OpFSub %float %[0-9]+ %[0-9]+
//
// f[i]++ for a vector element and --f[i].y for one of its lanes.
kernel void step_buffer_element_and_vector(device float4 *out [[buffer(0)]],
                                           uint i [[thread_position_in_grid]])
{
    out[i]++;
    --out[i].y;
}
