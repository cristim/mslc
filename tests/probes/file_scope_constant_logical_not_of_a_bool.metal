// EXPECT: valid
// DISASM: = OpConstantFalse %bool

//
// !true is false.
constant bool kValue = !true;

kernel void file_scope_constant_logical_not_of_a_bool(device uint* out [[buffer(0)]],
                                                      uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
    if (kValue) {
        out[i] = 1u;
    }
}
