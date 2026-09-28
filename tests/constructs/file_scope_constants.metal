// A file-scope "constant", in the three forms a shader uses: a literal, an
// expression over constants declared before it, and a struct given by field
// name, whose members are read in a body.
constant float kEtaAir = 1.000277;
constant float kEtaGlass = 1.5;

// Folding across declarations: this is the ratio of two constants above, and
// SPIR-V takes only constants as a constant's operands, so it has to be
// worked out here rather than emitted as a division.
constant float kEtaRatio = kEtaAir / kEtaGlass;

// A vector constant, whose components are positional.
constant float3 kSpecularColor = { 1, 1, 1 };
constant float kSpecularPower = 80;

struct Light
{
    float3 direction;
    float3 ambientColor;
};

constant Light light = {
    .direction = { 0.13, 0.72, 0.68 },
    .ambientColor = { 0.05, 0.05, 0.05 }
};

kernel void shade(device float *out [[buffer(0)]],
                  uint index [[thread_position_in_grid]])
{
    out[index] = light.ambientColor.x * kEtaRatio + kSpecularColor.y * kSpecularPower;
}
