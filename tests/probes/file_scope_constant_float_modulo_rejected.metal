// EXPECT: error this operator is recognised but not folded yet
//
// A float remainder is not folded, and is named rather than computed as an integer.
constant float kValue = 1.5f % 2.0f;

kernel void file_scope_constant_float_modulo_rejected(device float* out [[buffer(0)]],
                                                      uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
