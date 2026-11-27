// EXPECT: error an integer constant divides by zero
//
// The folder evaluates an integer initialiser at compile time, so "% 0" and
// "/ 0" are a division here rather than at run time. An integer division by zero
// is undefined, and on this host it would be a hardware trap, so it is reported
// rather than folded. A float is the other way round: 1.0 / 0.0 is infinity,
// which is what the language says, and is left alone.
constant int kZero = 0;
constant int kDivided = 5 / kZero;

kernel void file_scope_constant_divides_by_zero(device float* out [[buffer(0)]],
                                                uint index [[thread_position_in_grid]])
{
    out[index] = float(kDivided);
}
