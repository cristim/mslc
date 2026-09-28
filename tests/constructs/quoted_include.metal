// A header name is either <angled> or "quoted", and the quoted form is one
// token, so consuming it must not run on looking for a '>' that is not there.
// Everything after these two includes is the point of the fixture: mslc does not
// read a header, and this is the shader saying so by carrying on regardless of
// what the header would have declared. Any type or macro a header does declare
// is reported by name where it is used.
#include <metal_stdlib>
#include "AAPLShaderTypes.h"

using namespace metal;

struct Payload
{
    float4 position [[position]];
};

vertex Payload vertex_project(const device Payload *payloads [[buffer(0)]],
                              uint vid [[vertex_id]])
{
    return payloads[vid];
}
