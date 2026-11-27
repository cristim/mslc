// EXPECT: error does not fit int
//
// A float outside the integer's range is undefined to convert; Apple folds it through LLVM's
// rules, which mslc does not copy, so it is refused rather than given a value.
constant int4 k = int4(2147483648.0f);
kernel void file_scope_constant_int_vector_float_out_of_range_rejected(device int *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = k.x;
}
