// EXPECT: no module
// REFLECT-NOT: "name"
//
// Several helpers, one taking a struct, and still no entry point.
struct Light {
    float3 direction;
    float intensity;
};

float3 normalize_or_zero(float3 v) { return v; }
float luminance(Light l) { return l.intensity; }
float brightness(float3 c) { return c.x; }
