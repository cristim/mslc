// EXPECT: valid
// DISASM-NOT: OpVectorShuffle
// DISASM-MATCH: = OpAccessChain %_ptr_PhysicalStorageBuffer_float %[_0-9a-zA-Z]+ %uint_0[_0-9]* %[_0-9a-zA-Z]+ %uint_1[_0-9]*[^ _0-9a-zA-Z]
//
// A struct's own field called x is the field, not a swizzle of anything.
struct Point {
    float y;
    float x;
};
kernel void struct_field_named_like_a_swizzle(device float *out [[buffer(0)]], constant Point *p [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = p[i].x;
}
