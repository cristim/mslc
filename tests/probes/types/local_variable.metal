// EXPECT: valid
// DISASM: OpVariable %_ptr_Function_uint Function
kernel void local_variable(device uint* out [[buffer(0)]],
                           uint i [[thread_position_in_grid]])
{
    uint x = i * 2u;
    out[i] = x;
}
