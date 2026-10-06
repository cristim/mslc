// EXPECT: valid
//
// The right operand of && and || in an enumerator is evaluated only when the
// left does not decide the result, so a division by zero there is not reached.
enum { A = 0 && (1 / 0), B = 1 || (1 / 0) };
kernel void enum_logical_and_does_not_evaluate_a_decided_operand(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = uint(A + B);
}
