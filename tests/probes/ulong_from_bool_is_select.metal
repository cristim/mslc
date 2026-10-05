// EXPECT: valid
// DISASM-MATCH: = OpSelect %ulong %[_0-9a-zA-Z]+ %ulong_1 %ulong_0[^_0-9a-zA-Z]
//
// A 64-bit one is two literal words, low word first.
kernel void ulong_from_bool_is_select(device ulong *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool b = v[i] > 3u;
    out[i] = ulong(b);
}
