// EXPECT: valid
// An unscoped enum's enumerators are names of the enclosing namespace too, so
// the library's namespace has them directly. Apple accepts both.
#include <metal_stdlib>
using metal::memory_order_seq_cst;
using metal::round_to_nearest;
kernel void using_declaration_of_unscoped_enumerator_accepted(device float* out [[buffer(0)]])
{
    out[0] = 1.0f;
}
