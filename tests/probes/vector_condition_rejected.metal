// EXPECT: error a condition has to be a bool or a numeric scalar
//
// Apple rejects a vector as a condition, bool vector or not.
kernel void vector_condition_rejected(device uint *out [[buffer(0)]], constant int2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (v[i]) {
        out[i] = 1u;
    }
}
