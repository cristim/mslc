// EXPECT: error builtin "position" is not available in a vertex function
//
// A vertex function taking a direct [[position]] parameter that is also the
// wrong type (float2, not float4): the stage check has to fire before the
// float4 type check, so the diagnostic is the stage error, not a confusing
// type error, matching how declareStageStruct already orders this for the
// stage-struct path.
vertex float4 position_parameter_wrong_type_in_vertex_reports_stage_error(float2 p [[position]])
{
    return float4(p, 0.0, 1.0);
}
