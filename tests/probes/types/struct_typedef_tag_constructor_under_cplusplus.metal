// EXPECT: valid
// The shape of iTerm2's shared header: a tagged struct in a typedef whose
// constructor sits behind the macro Metal defines.
typedef struct Piu {
#ifdef __cplusplus
    Piu() {}
#endif
    float2 offset;
    float4 color;
} Piu;

kernel void k(device float *out [[buffer(0)]], device const Piu *p [[buffer(1)]])
{
    out[0] = p[0].offset.x + p[0].color.w;
}
