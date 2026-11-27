// EXPECT: error struct "Light" has no field "ambientt" where "ambient" goes
//
// A struct-valued constant is initialised by field name, so the list is checked
// against the declaration rather than taken positionally. A name that is not a
// field, or one in the wrong place, would otherwise be dropped and leave the
// field it should have initialised at zero: a direction of (0, 0, 0) lights
// nothing, and a specular power of 0 makes every highlight vanish.
struct Light
{
    float direction;
    float ambient;
};

constant Light light = {
    .direction = 0.13,
    .ambientt = 0.05
};

kernel void file_scope_constant_field_misnamed(device float* out [[buffer(0)]],
                                               uint index [[thread_position_in_grid]])
{
    out[index] = light.direction;
}
