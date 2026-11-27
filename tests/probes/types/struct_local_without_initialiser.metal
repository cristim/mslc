// EXPECT: valid
// DISASM-MATCH: %[0-9]+ = OpConstantNull %_struct_[0-9]+
// DISASM: OpAccessChain %_ptr_Function_float
//
// A struct local declared without an initialiser is stored the struct's
// OpConstantNull, which zeroes every field, and its fields are then written and
// read through access chains like any other local's.
struct Pair {
    float a;
    float b;
};

kernel void struct_local_without_initialiser(device float* out [[buffer(0)]],
                                             uint i [[thread_position_in_grid]])
{
    Pair p;
    p.a = 1.0;
    out[i] = p.a + p.b;
}
