// EXPECT: error cannot assign to a literal or an enumerator constant
//
// Apple: expression is not assignable.
enum Colour { Red = 1, Green = 2 };

kernel void enumerator_store_rejected(device float *out [[buffer(0)]])
{
    Red = 2;
    out[0] = 1.0;
}
