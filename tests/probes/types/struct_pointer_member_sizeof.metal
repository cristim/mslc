// EXPECT: valid
// DISASM: OpConvertUToF %float %ulong_16
// DISASM: OpConvertUToF %float %ulong_8
// An element of a pointer member is its pointee, a float3 of 16 bytes, and the member itself is 8.
struct S { device float3 *v; };
kernel void struct_pointer_member_sizeof(constant S &s [[buffer(0)]], device float *o [[buffer(1)]])
{
    o[0] = sizeof(s.v[0]);
    o[1] = sizeof(s.v);
}
