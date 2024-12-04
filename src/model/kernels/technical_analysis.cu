#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math.h>

extern "C" {

__global__ void moving_average_kernel(const double* prices,
                                    double* result,
                                    int period,
                                    int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size - period + 1) return;

    double sum = 0.0;
    for (int i = 0; i < period; i++) {
        sum += prices[idx + i];
    }
    result[idx] = sum / period;
}

__global__ void exponential_moving_average_kernel(const double* prices,
                                                double* result,
                                                double alpha,
                                                int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    if (idx == 0) {
        result[0] = prices[0];
    } else {
        result[idx] = alpha * prices[idx] + (1 - alpha) * result[idx - 1];
    }
}

__global__ void bollinger_bands_kernel(const double* prices,
                                     double* upperBand,
                                     double* middleBand,
                                     double* lowerBand,
                                     int period,
                                     double numStdDev,
                                     int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size - period + 1) return;

    // Calculate SMA
    double sum = 0.0;
    double sumSq = 0.0;
    for (int i = 0; i < period; i++) {
        double price = prices[idx + i];
        sum += price;
        sumSq += price * price;
    }
    
    double sma = sum / period;
    double variance = (sumSq - (sum * sum / period)) / (period - 1);
    double stdDev = sqrt(variance);
    
    middleBand[idx] = sma;
    upperBand[idx] = sma + numStdDev * stdDev;
    lowerBand[idx] = sma - numStdDev * stdDev;
}

__global__ void rsi_kernel(const double* prices,
                          double* rsi,
                          int period,
                          int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size - period) return;

    double sumGain = 0.0;
    double sumLoss = 0.0;

    // Calculate initial gains and losses
    for (int i = 1; i <= period; i++) {
        double diff = prices[idx + i] - prices[idx + i - 1];
        if (diff > 0) {
            sumGain += diff;
        } else {
            sumLoss -= diff;
        }
    }

    double avgGain = sumGain / period;
    double avgLoss = sumLoss / period;
    
    if (avgLoss == 0.0) {
        rsi[idx] = 100.0;
    } else {
        double rs = avgGain / avgLoss;
        rsi[idx] = 100.0 - (100.0 / (1.0 + rs));
    }
}

} // extern "C"
