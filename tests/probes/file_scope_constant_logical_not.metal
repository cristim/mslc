// EXPECT: valid
// DISASM: = OpConstantTrue %bool
//
// !0 is true. A bool is the result of !, so only a bool constant takes it.
constant bool kValue = !0;

kernel void file_scope_constant_logical_not(device uint* out [[buffer(0)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
    if (kValue) {
        out[i] = 1u;
    }
}
