// EXPECT: error a bool converts to and from a numeric scalar or vector only
//
// Apple has no conversion from a struct to bool. A compare of a struct with a
// null constant is a module spirv-val rejects.
struct Pair
{
    float a;
    float b;
};

kernel void bool_from_struct_rejected(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    Pair p;
    p.a = 1.0;
    p.b = 2.0;
    bool flag = bool(p);
    if (flag) {
        out[i] = 1u;
    }
}
