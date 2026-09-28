// A cube texture read along a direction, which is the whole difference between a
// texture2d and a texturecube: the sample is addressed by a vector rather than
// by a pair of coordinates, and what comes back is the texel on whichever of the
// six faces that vector points at.
//
// The image the harness binds is one flat colour per face, so a lookup that
// picks the wrong face, or reads a direction as a pair of coordinates, returns a
// colour that is on no face it was looking for. The directions point squarely at
// each face in turn, with the naming component at one and the other two at a
// third of it, so a direction is unambiguous and a swap of two components
// changes the face.
//
// The z component is negated on the way to the sample, which is what a skybox
// does and what makes the test catch a handedness mistake: a lookup that forgot
// the negation still returns a colour, just the colour of the opposite face.
//
// The sample is in a fragment stage because a read is implicit-LOD, and a
// compute stage has no derivatives to choose a level with. The vertex stage is
// here only to give the draw something to rasterise, and the harness issues one
// draw per face, so the direction comes from a uniform the host rewrites rather
// than from anything the shader computes.
// A struct with a [[position]] rather than a bare float4, because a vertex stage
// has to write gl_Position: a return type with no position member is a vertex
// output with a location, and a pipeline whose vertex stage has no position is
// not a pipeline.
struct Positioned
{
    float4 position [[position]];
};

vertex Positioned vertex_fullscreen(uint vid [[vertex_id]])
{
    // One oversized triangle covering the target, so every pixel is covered. A
    // branch rather than a conditional expression, which mslc has no syntax for
    // yet, and this is not the construct under test.
    float2 corner = float2(-1.0, -1.0);
    if (vid == 1) {
        corner.x = 3.0;
    }
    if (vid == 2) {
        corner.y = 3.0;
    }

    Positioned out;
    out.position = float4(corner.x, corner.y, 0.0, 1.0);
    return out;
}

fragment float4 cube_lookup(texturecube<float> cube [[texture(0)]],
                            sampler cubeSampler [[sampler(0)]],
                            constant float4 *direction [[buffer(0)]])
{
    float3 lookup = float3(direction[0].x, direction[0].y, -direction[0].z);
    return cube.sample(cubeSampler, lookup);
}
