// int - int4 broadcasts the scalar on the left and keeps it on the left; the
// operator is not commutative, so a swapped operand order changes the answer.
// Both operands are constants, so neither spelling emits a load between the
// broadcast and the operator and the ids line up.
constant int4 K = { 1, 2, 3, 4 };

kernel void k(device int4 *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = int4(7) - K;
}
