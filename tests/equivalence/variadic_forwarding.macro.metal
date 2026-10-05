#define CALL(fn, ...) fn(__VA_ARGS__)
#define CLAMPED(x, ...) CALL(clamp, x, __VA_ARGS__)
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = CLAMPED(i, 2u, 9u); }
