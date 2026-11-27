// EXPECT: valid
// DISASM-NO-MATCH: OpBitcast
struct Held
{
    float lead;
    packed_float3 a;
};

kernel void packed_local_struct_member_swizzle(device float3 *out [[buffer(0)]])
{
    Held loc;
    loc.a = packed_float3(1.0, 2.0, 3.0);
    float3 read = loc.a;
    out[0] = read + loc.a.zyx;
}
