// EXPECT: error local "s" of type Pair is initialised from another struct, or from one read straight from a buffer
//
// The element a buffer holds is laid out for an array and a local is not, so
// the copy is refused, whether the element is named "in[0]" or "*in".
struct Pair {
    float4 a;
    float b;
};

kernel void deref_whole_struct_into_a_local_rejected(device float* out [[buffer(0)]],
                                                     device const Pair* in [[buffer(1)]])
{
    Pair s = *in;
    out[0] = s.b;
}
