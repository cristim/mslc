// EXPECT: valid
// DISASM-MATCH: = OpIAdd %int %[0-9]+ %int_1
// DISASM-MATCH: = OpISub %int %[0-9]+ %int_1
//
// x++ and ++x are x += 1, and x-- and --x are x -= 1, when written as a statement.
kernel void step_statements_add_and_subtract_one(device int *out [[buffer(0)]],
                                                 uint i [[thread_position_in_grid]])
{
    int x = out[i];
    x++;
    ++x;
    x--;
    out[i] = x;
}
