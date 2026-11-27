// EXPECT: valid
// DISASM: OpConstant %ulong 8
// DISASM-NOT: OpConstant %ulong 4
//
// sizeof of a pointer parameter is 8 (Apple: 8), the size of the pointer, not of what it points
// at; the parser sees the parameter's own declaration, so a local of another type that hides it
// is sized as that local instead (sizeof_name_hiding).
kernel void sizeof_expression_pointer_parameter(device ulong *out [[buffer(0)]],
    device float *data [[buffer(1)]])
{
    out[0] = sizeof(data);
}
