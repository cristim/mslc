// EXPECT: error builtin "position" is not available in a vertex function
//
// A vertex function taking a direct [[position]] parameter: builtinAllowedInStage
// restricts Position to Stage::Fragment, and this is the only probe that
// specifically proves Position itself, not some other builtin, is still
// gated now that it shares FragCoord's lowering (regression guard; overlaps
// builtin_in_the_wrong_stage_rejected.metal, which pins a different builtin).
vertex float4 position_parameter_rejected_in_vertex(float4 p [[position]])
{
    return p;
}
