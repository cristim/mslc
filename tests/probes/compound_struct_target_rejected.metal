// EXPECT: error is lowered only for numeric scalars, vectors and matrices
//
// Apple: no viable overloaded '+='.
struct Pair
{
    float a;
    float b;
};

kernel void compound_struct_target_rejected(device Pair *out [[buffer(0)]],
                                            uint i [[thread_position_in_grid]])
{
    Pair x;
    x.a = 1.0f;
    x += 1;
    out[i].a = x.a;
}
