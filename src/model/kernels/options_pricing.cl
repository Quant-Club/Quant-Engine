// Options Pricing Kernels

#define M_PI 3.14159265358979323846

double normalCDF(double x) {
    return 0.5 * (1.0 + erf(x / sqrt(2.0)));
}

__kernel void black_scholes(
    __global const OptionData* options,
    __global double* callPrices,
    __global double* putPrices
) {
    int idx = get_global_id(0);
    if (idx >= get_global_size(0)) return;

    OptionData opt = options[idx];
    
    double sqrtTime = sqrt(opt.timeToExpiry);
    double d1 = (log(opt.spotPrice / opt.strikePrice) + 
                (opt.riskFreeRate + 0.5 * opt.volatility * opt.volatility) * opt.timeToExpiry) / 
                (opt.volatility * sqrtTime);
    double d2 = d1 - opt.volatility * sqrtTime;

    double nd1 = normalCDF(d1);
    double nd2 = normalCDF(d2);
    double nnd1 = normalCDF(-d1);
    double nnd2 = normalCDF(-d2);

    double discountFactor = exp(-opt.riskFreeRate * opt.timeToExpiry);
    
    callPrices[idx] = opt.spotPrice * nd1 - opt.strikePrice * discountFactor * nd2;
    putPrices[idx] = opt.strikePrice * discountFactor * nnd2 - opt.spotPrice * nnd1;
}

// Box-Muller transform for generating normal random numbers
double2 boxMuller(double u1, double u2) {
    double r = sqrt(-2.0 * log(u1));
    double theta = 2.0 * M_PI * u2;
    return (double2)(r * cos(theta), r * sin(theta));
}

__kernel void monte_carlo(
    __global const SimulationParams* params,
    __global double* paths,
    const int numPaths,
    const int numSteps
) {
    int pathIdx = get_global_id(0);
    if (pathIdx >= numPaths) return;

    SimulationParams param = params[get_global_id(1)];
    double dt = param.timeHorizon / numSteps;
    double sqrtDt = sqrt(dt);
    
    int baseIdx = (get_global_id(1) * numPaths + pathIdx) * numSteps;
    paths[baseIdx] = param.spotPrice;

    // Use different seeds for different paths
    ulong seed = (ulong)pathIdx * get_global_id(1);
    
    for (int step = 1; step < numSteps; step++) {
        // Generate random numbers using Box-Muller transform
        seed = (seed * 1664525 + 1013904223);
        double u1 = (double)(seed & 0xFFFFFFFF) / 0xFFFFFFFF;
        seed = (seed * 1664525 + 1013904223);
        double u2 = (double)(seed & 0xFFFFFFFF) / 0xFFFFFFFF;
        
        double z = boxMuller(u1, u2).x;
        
        double drift = (param.riskFreeRate - 0.5 * param.volatility * param.volatility) * dt;
        double diffusion = param.volatility * sqrtDt * z;
        
        paths[baseIdx + step] = paths[baseIdx + step - 1] * exp(drift + diffusion);
    }
}
