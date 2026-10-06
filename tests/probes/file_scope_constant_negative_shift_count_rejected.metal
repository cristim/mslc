// EXPECT: error a shift of a constant is by a count outside 0 to 31
//
// A negative count is undefined too.
constant int kValue = 1 >> -1;

kernel void file_scope_constant_negative_shift_count_rejected(device int* out [[buffer(0)]],
                                                              uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
