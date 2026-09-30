// EXPECT: valid
// DISASM: OpCompositeConstruct %v3float
// DISASM-NOT: OpFConvert %v3float
//
// The same construction in expression position, float3(0) on the right of an
// assignment. This spelling already parsed before, as a cast, and then failed in
// lowering with "a scalar cannot be converted to a vector; mslc builds a vector
// from a list of values, which it does not do yet", which was the right
// diagnostic for a conversion and the wrong one for a constructor.
//
// Metal has no cast syntax, so T(...) is always a constructor call. A node
// modelled on a cast could hold one operand and no list, which is why the
// zero-argument form could not even be parsed ("unexpected )"), and why a list
// like float4(a, b, c, 1) had nowhere to go. This is the node the list form
// will arrive on.
kernel void construct_broadcast(device float3 *out [[buffer(0)]],
                                constant float3 *in [[buffer(1)]],
                                uint index [[thread_position_in_grid]])
{
    float3 copied = in[index];
    out[index] = copied + float3(0);
}
