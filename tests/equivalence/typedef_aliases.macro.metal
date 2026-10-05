#include <metal_stdlib>
using namespace metal;

typedef uint index_t;
typedef index_t slot_t;
typedef float3 vec3_t;
typedef float4x4 mat_t;
typedef const float kScale;
typedef struct Pair { float a; float b; } PairAlias;

enum { First, Second, Third = 7, Neg = -2 };
constant uint kThird = Third;

kernel void typedef_aliases(device float *out [[buffer(Second)]],
                            constant PairAlias *pairs [[buffer(First)]],
                            slot_t i [[thread_position_in_grid]])
{
    kScale s = 2.0;
    vec3_t v = vec3_t(pairs[i].a, pairs[i].b, s);
    mat_t m = mat_t(float4(1.0), float4(2.0), float4(3.0), float4(4.0));
    index_t n = kThird + Third;
    int k = Neg * 3;
    out[i] = v.x + v.y + v.z + m[Second].x + float(n) + float(k);
}
