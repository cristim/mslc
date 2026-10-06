// EXPECT: valid
// DISASM-MATCH: = OpAccessChain %_ptr_PhysicalStorageBuffer_int
// DISASM-MATCH: = OpAccessChain %_ptr_Function_int
//
// Assigning to one lane of a struct member, a buffer element and a local, by a
// constant and by a run-time index, stores to that lane only (#53).
struct S { int4 a; int4 b; };

kernel void vector_element_write_member_and_buffer(device int4 *out [[buffer(0)]],
                                                   device S *s [[buffer(1)]],
                                                   device const uint *ix [[buffer(2)]])
{
    int4 v = out[0];
    v[ix[0]] = 5;
    s[0].b[2] = 7;
    s[0].a[ix[1]]++;
    out[1][ix[2]] -= 3;
    out[2] = v;
}
