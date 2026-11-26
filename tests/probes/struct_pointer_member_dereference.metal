// EXPECT: valid
// DISASM-MATCH: OpLoad %_ptr_PhysicalStorageBuffer__struct_[0-9]+ %[0-9]+ Aligned 8
// DISASM-MATCH: OpStore %[0-9]+ %[0-9]+ Aligned 16
// *s.p is element zero of what p points at, and a swizzle of an element is read and written as of any
// buffer element.
struct S { device float4 *p; constant float *c; };
kernel void struct_pointer_member_dereference(constant S &s [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    *s.p = float4(*s.c);
    s.p[i].yz += float2(s.c[i], 1);
}
