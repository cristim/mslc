// EXPECT: valid
// DISASM-MATCH: OpIAdd %int
// ushort + ushort is an int in Metal, so 65535 + 1 is 65536 and not a wrapped ushort.
kernel void auto_local_small_integers_promote_to_int(device float *o [[buffer(0)]]) {
  auto s = ushort(65535) + ushort(1);
  o[0] = s;
}
