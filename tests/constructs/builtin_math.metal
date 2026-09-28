// MSL's builtin math functions, which are GLSL.std.450 extended instructions
// rather than core opcodes, plus the two that are not extended instructions at
// all: dot is a core OpDot because GLSL.std.450 has no Dot, and Metal's saturate
// is a clamp between zero and one, which is FClamp with bounds the source never
// wrote down.
//
// The argument order is the part that cannot be checked by a validator. GLSL
// GLSL.std.450 has FMin and FMax as separate instructions, and Swapping the two
// gives a module that validates perfectly and returns the larger number from
// min. So min and max are both here, with inputs where the two disagree, and so
// are reflect and refract, whose incident and normal are in the opposite order
// from a reader's first guess.
//
// The values are exact and the operations are checked numerically against a host
// reference by the readback harness, so a wrong instruction, a wrong operand
// order and a wrong scale factor all show up as a different number rather than
// as a different module.
struct Composed
{
    float4 normalized;
    float4 clamped;
    float4 reflected;
    float4 refracted;
    float4 smallest;
    float4 largest;
    float4 powered;
    float4 dotted;
};

kernel void builtin_math(device Composed *out [[buffer(0)]],
                         constant float4 *values [[buffer(1)]],
                         uint index [[thread_position_in_grid]])
{
    // A length that is neither 1 nor a power of two, so a scale factor of 1 or
    // 1/2 gives a different answer rather than a rounded one.
    float4 v = values[0];
    float4 normal = normalize(v);

    // saturate on a vector and on a scalar: the bounds have to take the operand's
    // own shape, since FClamp requires all three operands to have the result
    // type. The scalar is a third rather than a zero or a one, and it scales the
    // vector's own result, so a wrong bound on either shows up in the output
    // rather than being multiplied away.
    float4 clamped = saturate(v * 4.0);
    float scalarClamp = saturate(v.z * 0.4);

    // reflect(I, N) is I - 2 * dot(N, I) * N, and refract(I, N, eta) is the
    // transmitted vector; the incident and the normal are in that order, and the
    // two are not interchangeable, so the normal here is not the direction of
    // the incident. A normal parallel to the incident would make reflect(I, N)
    // and reflect(N, I) the same value and hide a swapped pair.
    float4 surface = normalize(values[1]);
    float4 reflected = reflect(v, surface);
    float4 refracted = refract(v, surface, 0.5);

    float4 smallest = min(v, values[1]);
    float4 largest = max(v, values[1]);
    // The base is squared first because pow is undefined for a negative base in
    // GLSL.std.450: a host and a GPU may legitimately answer differently, so an
    // input that makes the result undefined cannot test the instruction.
    float4 powered = pow(v * v, values[1]);
    float dotted = dot(v, values[1]);

    out[index].normalized = normal;
    out[index].clamped = clamped * float4(scalarClamp);
    out[index].reflected = reflected;
    out[index].refracted = refracted;
    out[index].smallest = smallest;
    out[index].largest = largest;
    out[index].powered = powered;
    out[index].dotted = float4(dotted);
}
