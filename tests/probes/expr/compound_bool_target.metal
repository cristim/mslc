// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
//
// A compound assignment to a bool follows the binary operator on the promoted operands and converts the
// result back to bool, as Apple does. (This probe pinned the refusal until it was lowered.)
kernel void compound_bool_target(device int *out [[buffer(0)]],
                                          uint i [[thread_position_in_grid]])
{
    bool x = out[i] > 0;
    x &= 1;
    out[i] = x;
}
