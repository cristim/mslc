// EXPECT: error unsupported attribute "point_size"
vertex float4 vertex_point_size_parameter_rejected(float s [[point_size]]) { return float4(s); }
