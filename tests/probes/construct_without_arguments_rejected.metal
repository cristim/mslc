// EXPECT: error float3() has no arguments and mslc has no default value to put in it
//
// xcrun metal accepts "float3 v();" and "float3 v = float3();" and zero-fills
// the vector. mslc reports instead, and the disagreement is deliberate: a
// zero-filled float3 in the emitted module is indistinguishable from
// "float3 v(0);", so a reader of the SPIR-V cannot tell a value someone wrote
// from one mslc invented. Fabricating the zero would also make the two spellings
// mean different things in mslc and the same thing on the GPU.
//
// The diagnostic names the value to write instead, so the fix is in the message.
kernel void construct_without_arguments_rejected(device float3 *out [[buffer(0)]],
                                                 uint index [[thread_position_in_grid]])
{
    float3 v();
    out[index] = v;
}
