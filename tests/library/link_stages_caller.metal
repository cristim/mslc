// One import shared by two stages: declared once for the module, not once per stage.
#include <metal_stdlib>
using namespace metal;

float shade(float value);

vertex float4 vmain(uint vid [[vertex_id]]) {
	return float4(shade(1.0));
}

fragment float4 fmain() {
	return float4(shade(2.0));
}
