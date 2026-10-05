#define VARIABLES(X) X(first, 1u) X(second, 2u) X(third, 3u)
#define DECLARE(name, value) uint name = value;
#define SUM(name, value) + name
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    VARIABLES(DECLARE)
    out[i] = 0u VARIABLES(SUM);
}
