// EXPECT: valid
// DISASM-NO-MATCH: OpBitcast
//
// A local struct is laid out by nothing, so its packed member is a plain vector
// in Function storage and is reached without the buffer's array-of-components
// storage.
struct Held
{
    packed_float3 a;
};

kernel void packed_local_struct_member(device float *out [[buffer(0)]])
{
    Held loc;
    loc.a = packed_float3(1.0, 2.0, 3.0);
    loc.a.y = 2.0;
    out[0] = loc.a.y;
}
