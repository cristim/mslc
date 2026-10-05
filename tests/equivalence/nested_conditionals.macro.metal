#define LEVEL 2
#define FAST
kernel void equivalent(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint v = i;
#if defined(FAST) && LEVEL >= 2
#  if LEVEL == 3
    v = v * 3u;
#  elif LEVEL == 2
    v = v * 2u;
#  else
    v = v;
#  endif
#else
    v = v + 100u;
#endif
    out[i] = v;
}
