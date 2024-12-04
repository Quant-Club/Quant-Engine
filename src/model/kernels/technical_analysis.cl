// Technical Analysis Kernels

__kernel void moving_average(
    __global const double* prices,
    __global double* result,
    const int period
) {
    int idx = get_global_id(0);
    int size = get_global_size(0) + period - 1;
    if (idx >= size - period + 1) return;

    double sum = 0.0;
    for (int i = 0; i < period; i++) {
        sum += prices[idx + i];
    }
    result[idx] = sum / period;
}

__kernel void exponential_moving_average(
    __global const double* prices,
    __global double* result,
    const double alpha
) {
    int idx = get_global_id(0);
    if (idx >= get_global_size(0)) return;

    if (idx == 0) {
        result[0] = prices[0];
    } else {
        result[idx] = alpha * prices[idx] + (1.0 - alpha) * result[idx - 1];
    }
}

__kernel void bollinger_bands(
    __global const double* prices,
    __global double* upperBand,
    __global double* middleBand,
    __global double* lowerBand,
    const int period,
    const double numStdDev
) {
    int idx = get_global_id(0);
    int size = get_global_size(0) + period - 1;
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

__kernel void rsi(
    __global const double* prices,
    __global double* rsi,
    const int period
) {
    int idx = get_global_id(0);
    if (idx >= get_global_size(0)) return;

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
