// EXPECT: valid
// DISASM: OpStore
//
// "device float * const o" makes the pointer const and not what it points at,
// so the store through it is allowed, as it is in Apple's compiler.
kernel void pointer_that_is_itself_const_still_stores(device float * const o [[buffer(0)]])
{
    o[0] = 1.0;
}
