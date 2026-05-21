#ifndef __RANDHELP__
#define __RANDHELP__

// Helper: random float in [lo, hi]
static float rand_range(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
}

static int rand_int(int cval) {
    int temps = static_cast<int>((std::rand()) / static_cast<float>(RAND_MAX) * (float)cval) -1;
    if(temps < 0) return 0;
    return temps;
}


#endif