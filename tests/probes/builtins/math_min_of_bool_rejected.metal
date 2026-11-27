// EXPECT: error min takes numeric scalars or vectors
// A bool has no GLSL.std.450 min; picking UMin for it would not validate.
kernel void math_min_of_bool_rejected(
    device const int* s [[buffer(0)]],
    device int* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int(min(s[i] < 0, s[i] > 0));
}
