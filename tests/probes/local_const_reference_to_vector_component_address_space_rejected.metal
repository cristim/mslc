// EXPECT: error address space
// The copy a const reference to a vector component binds is in thread memory; Apple refuses
// `const device float &r = b[0].x;` as an address-space mismatch.
kernel void k(device float4 *b [[buffer(0)]], device float *o [[buffer(1)]]) {
  const device float &r = b[0].x;
  o[0] = r;
}
