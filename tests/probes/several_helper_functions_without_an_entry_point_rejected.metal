// EXPECT: error declares 3 helper functions and no kernel, vertex or fragment entry point
//
// The plural of the probe above, pinned because the count reaches the message
// and a singular/plural slip would otherwise go unnoticed: the same source with
// one helper is the other probe.
struct Light {
    float3 direction;
    float intensity;
};

float3 normalize_or_zero(float3 v) { return v; }
float luminance(Light l) { return l.intensity; }
float brightness(float3 c) { return c.x; }
