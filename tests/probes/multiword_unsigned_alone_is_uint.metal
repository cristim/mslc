// EXPECT: valid
// DISASM: OpVariable %_ptr_Function_uint Function
//
// unsigned alone is unsigned int. Apple: sizeof 4, (unsigned)-1 > 0.
kernel void k(device uint* o [[buffer(0)]])
{
    unsigned a = 7;
    o[0] = a;
}
