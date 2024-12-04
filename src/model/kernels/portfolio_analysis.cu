#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math.h>

extern "C" {

__global__ void portfolio_optimization_kernel(const double* returns,
                                           const double* covariance,
                                           double* weights,
                                           double riskFreeRate,
                                           double targetReturn,
                                           int numAssets) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numAssets) return;

    // Simple implementation of Mean-Variance Optimization using gradient descent
    // Note: In practice, you would want to use a more sophisticated optimization algorithm
    
    const double learningRate = 0.01;
    const int maxIterations = 1000;
    const double epsilon = 1e-6;

    // Initialize weights equally
    weights[idx] = 1.0 / numAssets;
    __syncthreads();

    for (int iter = 0; iter < maxIterations; iter++) {
        // Calculate portfolio return and risk
        double portReturn = 0.0;
        double portRisk = 0.0;
        
        for (int i = 0; i < numAssets; i++) {
            portReturn += weights[i] * returns[i];
            for (int j = 0; j < numAssets; j++) {
                portRisk += weights[i] * weights[j] * covariance[i * numAssets + j];
            }
        }

        // Calculate gradients
        double returnGrad = returns[idx] - targetReturn;
        double riskGrad = 0.0;
        for (int j = 0; j < numAssets; j++) {
            riskGrad += weights[j] * covariance[idx * numAssets + j];
        }

        // Update weights
        double grad = returnGrad + riskGrad;
        weights[idx] -= learningRate * grad;

        // Project weights to satisfy constraints
        double sum = 0.0;
        for (int i = 0; i < numAssets; i++) {
            weights[i] = max(0.0, weights[i]); // Non-negativity constraint
            sum += weights[i];
        }
        weights[idx] /= sum; // Sum to 1 constraint

        __syncthreads();

        // Check convergence
        if (abs(grad) < epsilon) {
            break;
        }
    }
}

__global__ void value_at_risk_kernel(const double* returns,
                                   const double* weights,
                                   double* var,
                                   double confidence,
                                   int horizon,
                                   int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    // Calculate portfolio returns
    double portReturn = 0.0;
    for (int i = 0; i < size; i++) {
        portReturn += returns[i] * weights[i];
    }

    // Sort returns (simple bubble sort for demonstration)
    // In practice, use more efficient sorting algorithms
    __shared__ double sortedReturns[1024];
    sortedReturns[idx] = returns[idx];
    __syncthreads();

    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (sortedReturns[j] > sortedReturns[j + 1]) {
                double temp = sortedReturns[j];
                sortedReturns[j] = sortedReturns[j + 1];
                sortedReturns[j + 1] = temp;
            }
        }
    }
    __syncthreads();

    // Calculate VaR
    if (idx == 0) {
        int varIndex = (int)((1.0 - confidence) * size);
        *var = -sortedReturns[varIndex] * sqrt(horizon);
    }
}

} // extern "C"
