// EXPECT: valid
// DISASM-CAPTURE: %(_struct_[0-9]+) = OpTypeStruct %_ptr_PhysicalStorageBuffer__struct_[0-9]+ %_ptr_PhysicalStorageBuffer__struct_[0-9]+ %float
// DISASM-MATCH-CAPTURED: OpMemberDecorate %$1 1 Offset 8
// DISASM-MATCH-CAPTURED: OpMemberDecorate %$1 2 Offset 16
// DISASM-MATCH-CAPTURED: OpDecorate %_runtimearr_$1 ArrayStride 24
// DISASM: OpDecorate %_runtimearr__arr_float_uint_3 ArrayStride 12
// A struct with pointer members is 8-aligned, so { p, c, q } is 24 bytes in an array of it, and a
// pointer to packed_float3 steps 12.
struct S { device float4 *p; constant packed_float3 *c; float q; };
kernel void struct_pointer_member_array_of_structs(device S *s [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    s[i].p[i] = float4(s[i].c[i], s[i].q);
    s->p[0].w = 0;
}
