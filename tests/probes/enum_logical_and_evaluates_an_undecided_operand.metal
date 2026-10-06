// EXPECT: error division by zero in a constant expression
//
// 1 && (1 / 0) needs its right operand, so the division is evaluated.
enum { A = 1 && (1 / 0) };
kernel void enum_logical_and_evaluates_an_undecided_operand(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = uint(A);
}
