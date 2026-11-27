// EXPECT: error a shift of a constant is by a count outside 0 to 31
//
// A shift by the width or more is undefined, and Apple folds each such count to
// a different value, so it is rejected.
constant int kValue = 1 << 32;

kernel void file_scope_constant_shift_count_past_the_width_rejected(device int* out [[buffer(0)]],
                                                                    uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
