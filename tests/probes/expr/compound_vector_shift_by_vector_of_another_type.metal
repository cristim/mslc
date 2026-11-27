// EXPECT: valid
// DISASM-MATCH: = OpShiftRightLogical %v4uint %[0-9]+ %[0-9]+
//
// uint4 v; v >>= int4 shifts lane by lane. Apple's compiler takes a count vector of any integer
// type, where every other operator wants the vector's own type.
kernel void compound_vector_shift_by_vector_of_another_type(device uint4 *out [[buffer(0)]], device const int4 *n [[buffer(1)]],
                                                            uint i [[thread_position_in_grid]])
{
    uint4 x = out[i];
    x >>= n[i];
    out[i] = x;
}
