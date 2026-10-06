// EXPECT: valid
// REFLECT: "metal_index": 0
// REFLECT: "metal_index": 3
//
// The right operand of && and || in an enumerator is evaluated only when the
// left does not decide the result, so a division by zero there is not reached.
// A is 0 and B is 1, so the two buffers take indices 0 and 1 + 2.
enum { A = 0 && (1 / 0), B = 1 || (1 / 0) };
kernel void enum_logical_and_does_not_evaluate_a_decided_operand(device uint *a [[buffer(A)]], device uint *b [[buffer(B + 2)]], uint i [[thread_position_in_grid]])
{
    a[i] = b[i];
}
