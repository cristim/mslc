// EXPECT: error the value fragment function "fragment_direct_ushort_rejected" returns is a ushort, which mslc does not pass between stages yet
fragment ushort fragment_direct_ushort_rejected() {
    return ushort(7);
}
