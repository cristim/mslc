// EXPECT: valid
// DISASM: OpStore
//
// A copy of a const object is a new object: "float y = x" is writable even
// when x is "float const".
kernel void trailing_const_copy_of_const_local_is_writable(device float* o [[buffer(0)]])
{
    float const x = 1.0;
    float y = x;
    y = 2.0;
    o[0] = x + y;
}
