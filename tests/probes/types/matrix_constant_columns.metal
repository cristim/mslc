// EXPECT: valid
// DISASM-MATCH: OpConstantComposite %mat2v2float %[0-9a-z_]+ %[0-9a-z_]+
// DISASM-NOT: OpMatrixTimesVector %float
// A file-scope matrix constant from one vector constructor per column, a braced list of
// column vectors, and a braced list of braced columns.
constant float2x2 rotation = { float2(1.0, 0.0), float2(0.0, 1.0) };
constant float2x2 nested = { { 1.0, 2.0 }, { 3.0, 4.0 } };
constant float2x2 columns = float2x2(float2(1.0, 2.0), float2(3.0, 4.0));

kernel void matrix_constant_columns(device float2 *out [[buffer(0)]],
                                    uint i [[thread_position_in_grid]])
{
    out[i] = rotation * float2(1.0) + nested[1] + columns * float2(1.0);
}
