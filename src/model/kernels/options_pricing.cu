#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math.h>
#include <curand_kernel.h>

extern "C" {

__device__ double normalCDF(double x) {
    return 0.5 * (1.0 + erf(x / sqrt(2.0)));
}

__global__ void black_scholes_kernel(const OptionData* options,
                                   double* callPrices,
                                   double* putPrices,
                                   int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    const OptionData& opt = options[idx];
    
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

__global__ void monte_carlo_kernel(const SimulationParams* params,
                                 double* paths,
                                 int numPaths,
                                 int numSteps,
                                 unsigned long long seed) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numPaths) return;

    curandState state;
    curand_init(seed, idx, 0, &state);

    const SimulationParams& param = params[blockIdx.y];
    double dt = param.timeHorizon / numSteps;
    double sqrtDt = sqrt(dt);
    
    int baseIdx = (blockIdx.y * numPaths + idx) * numSteps;
    paths[baseIdx] = param.spotPrice;

    for (int step = 1; step < numSteps; step++) {
        double z = curand_normal(&state);
        double drift = (param.riskFreeRate - 0.5 * param.volatility * param.volatility) * dt;
        double diffusion = param.volatility * sqrtDt * z;
        
        paths[baseIdx + step] = paths[baseIdx + step - 1] * exp(drift + diffusion);
    }
}

} // extern "C"
