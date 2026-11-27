// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpIAdd
//
// The value of an assignment is its left operand once stored to ([expr.ass]): out[i] gets x + 1.
// (This probe pinned the refusal of an assignment used as a value until it was lowered.)
kernel void compound_assignment_as_a_value(device int *out [[buffer(0)]],
                                           uint i [[thread_position_in_grid]])
{
    int x = out[i];
    out[i] = (x += 1);
}
