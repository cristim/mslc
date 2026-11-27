// EXPECT: valid
// DISASM: OpAccessChain
// A run-time column of a constant matrix goes through a copy in a Function variable, since a
// constant composite has no run-time extract.
constant float3x3 table = float3x3(1, 2, 3, 4, 5, 6, 7, 8, 9);

kernel void matrix_constant_dynamic_column(device float3 *out [[buffer(0)]],
                                           uint i [[thread_position_in_grid]])
{
    out[i] = table[i % 3];
}
