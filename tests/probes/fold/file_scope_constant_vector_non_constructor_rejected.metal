// EXPECT: error not a single value, another constant or an expression
//
// Apple accepts "constant float4 c = a;" and "-float4(1)"; mslc folds a list or a constructor
// of the vector's own type only, and says so.
constant float4 a = float4(1);
constant float4 c = -float4(1);
kernel void file_scope_constant_vector_non_constructor_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = c.x + a.x;
}
