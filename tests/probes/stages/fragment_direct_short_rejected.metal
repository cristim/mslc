// EXPECT: error the value fragment function "fragment_direct_short_rejected" returns is a short, which mslc does not pass between stages yet
fragment short fragment_direct_short_rejected() {
    return short(7);
}
