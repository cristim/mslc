// EXPECT: valid
// DISASM-MATCH: = OpFOrdGreaterThan %v2bool
//
// A literal beside a float vector is converted to the component type.
kernel void comparison_vector_literal_converts_to_component(device int2 *out [[buffer(0)]],
    constant float2 *a [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int2(a[i] > 1);
}
