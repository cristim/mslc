// A vector built from a list of values, in the two spellings Metal uses for it:
// as a constructor expression, "float4(1, 2, 3, 4)", and as direct
// initialisation of a declaration, "float3 specularTerm(0)". The second is the
// same expression, so it is read as one rather than as a second form.
//
// The count is checked against the width rather than padded or truncated:
// Metal has no way to build a float4 from three values, and silently filling
// the fourth would be a value the shader never asked for.
//
// One local per entry point, which is a limit of mslc's rather than of the
// form: a function's variables have to be the first instructions of its first
// block, and mslc emits each one where it is declared, so a second local
// declared after an initialiser has been computed lands in the wrong place.
// The outputs are float4 rather than the narrower vectors the widths here
// suggest, because an array of a three-wide vector is a stride Vulkan's
// relaxed storage buffer layout does not allow, which is a separate gap.
kernel void direct(device float4 *out [[buffer(0)]],
                   constant float4 *in [[buffer(1)]],
                   uint index [[thread_position_in_grid]])
{
    // Direct initialisation, which is how a shader declares a vector it then
    // uses: one value, spread across the components.
    float3 zeroed(0);
    out[index] = float4(zeroed, 1.0) + in[index];
}

kernel void positional(device float4 *out [[buffer(0)]],
                       constant float4 *in [[buffer(1)]],
                       uint index [[thread_position_in_grid]])
{
    // A value per component, positional.
    float4 explicit4(1.0, 2.0, 3.0, 4.0);
    out[index] = explicit4 + in[index];
}
