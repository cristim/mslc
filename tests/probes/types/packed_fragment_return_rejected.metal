// EXPECT: error is a packed_float4, which Apple's compiler does not allow across a stage boundary
//
// Apple rejects a packed type as a stage function's result and in its input and
// output structs, so mslc names it rather than give it an interface location.
fragment packed_float4 packed_fragment_return_rejected()
{
    return packed_float4(1.0, 2.0, 3.0, 4.0);
}
