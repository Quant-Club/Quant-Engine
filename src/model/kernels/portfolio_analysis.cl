// Portfolio Analysis Kernels

__kernel void portfolio_optimization(
    __global const double* returns,
    __global const double* covariance,
    __global double* weights,
    const double riskFreeRate,
    const double targetReturn,
    const int numAssets
) {
    int idx = get_global_id(0);
    if (idx >= numAssets) return;

    // Simple implementation of Mean-Variance Optimization using gradient descent
    // Note: In practice, you would want to use a more sophisticated optimization algorithm
    
    const double learningRate = 0.01;
    const int maxIterations = 1000;
    const double epsilon = 1e-6;

    // Initialize weights equally
    weights[idx] = 1.0 / numAssets;
    barrier(CLK_GLOBAL_MEM_FENCE);

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

        barrier(CLK_GLOBAL_MEM_FENCE);

        // Check convergence
        if (fabs(grad) < epsilon) {
            break;
        }
    }
}

__kernel void value_at_risk(
    __global const double* returns,
    __global const double* weights,
    __global double* var,
    const double confidence,
    const int horizon,
    const int size
) {
    int idx = get_global_id(0);
    if (idx >= size) return;

    // Calculate portfolio returns
    double portReturn = 0.0;
    for (int i = 0; i < size; i++) {
        portReturn += returns[i] * weights[i];
    }

    // Sort returns (simple bubble sort for demonstration)
    // In practice, use more efficient sorting algorithms
    __local double sortedReturns[1024];
    sortedReturns[idx] = returns[idx];
    barrier(CLK_LOCAL_MEM_FENCE);

    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (sortedReturns[j] > sortedReturns[j + 1]) {
                double temp = sortedReturns[j];
                sortedReturns[j] = sortedReturns[j + 1];
                sortedReturns[j + 1] = temp;
            }
        }
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    // Calculate VaR
    if (idx == 0) {
        int varIndex = (int)((1.0 - confidence) * size);
        *var = -sortedReturns[varIndex] * sqrt(horizon);
    }
}
