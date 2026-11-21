// Overloaded helpers in a library (#266): each signature of an overloaded name is exported under
// its own Linkage name, and a name with one signature keeps its plain one.
#include <metal_stdlib>
using namespace metal;

float mixed(float x) {
	return x * 2.0;
}

float mixed(float3 x) {
	return mixed(x.x) + mixed(x.y) + mixed(x.z);
}

float mixed(float x, const thread int &n) {
	return x + float(n);
}

float solo(float x) {
	return x;
}
