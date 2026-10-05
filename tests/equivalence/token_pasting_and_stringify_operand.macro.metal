#define FIELD(n) value_##n
#define DECLARE(n, v) uint FIELD(n) = v;
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    DECLARE(a, 5u)
    DECLARE(b, 6u)
    out[i] = FIELD(a) + FIELD(b);
}
