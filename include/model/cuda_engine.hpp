#pragma once

#include <cuda_runtime.h>
#include <cuda.h>
#include <memory>
#include <string>
#include <stdexcept>
#include "compute_engine.hpp"
#include "common/logger.hpp"

namespace quant_hub {
namespace model {

class CudaEngine : public ComputeEngine {
public:
    static std::shared_ptr<CudaEngine> create() {
        return std::shared_ptr<CudaEngine>(new CudaEngine());
    }

    ~CudaEngine() override {
        if (isInitialized()) {
            shutdown();
        }
    }

    void initialize() override {
        if (isInitialized_) return;

        try {
            // Initialize CUDA
            cudaError_t error = cudaSetDevice(0);
            if (error != cudaSuccess) {
                throw std::runtime_error("Failed to set CUDA device: " + 
                                       std::string(cudaGetErrorString(error)));
            }

            // Get device properties
            error = cudaGetDeviceProperties(&deviceProps_, 0);
            if (error != cudaSuccess) {
                throw std::runtime_error("Failed to get device properties: " + 
                                       std::string(cudaGetErrorString(error)));
            }

            isInitialized_ = true;
            LOG_INFO("CUDA engine initialized on device: ", deviceProps_.name);

        } catch (const std::exception& e) {
            LOG_ERROR("CUDA initialization failed: ", e.what());
            throw;
        }
    }

    bool isInitialized() const override {
        return isInitialized_;
    }

    void shutdown() override {
        if (!isInitialized_) return;

        try {
            if (deviceMemory_) {
                cudaFree(deviceMemory_);
                deviceMemory_ = nullptr;
            }

            cudaDeviceReset();
            isInitialized_ = false;
            LOG_INFO("CUDA engine shutdown complete");

        } catch (const std::exception& e) {
            LOG_ERROR("CUDA shutdown failed: ", e.what());
            throw;
        }
    }

    void allocateMemory(size_t size) override {
        if (!isInitialized_) {
            throw std::runtime_error("CUDA engine not initialized");
        }

        if (deviceMemory_) {
            cudaFree(deviceMemory_);
        }

        cudaError_t error = cudaMalloc(&deviceMemory_, size);
        if (error != cudaSuccess) {
            throw std::runtime_error("Failed to allocate CUDA memory: " + 
                                   std::string(cudaGetErrorString(error)));
        }

        allocatedSize_ = size;
        LOG_INFO("Allocated ", size, " bytes on CUDA device");
    }

    void freeMemory() override {
        if (deviceMemory_) {
            cudaFree(deviceMemory_);
            deviceMemory_ = nullptr;
            allocatedSize_ = 0;
        }
    }

    void copyToDevice(const void* hostData, size_t size) override {
        if (!deviceMemory_) {
            throw std::runtime_error("No device memory allocated");
        }

        if (size > allocatedSize_) {
            throw std::runtime_error("Copy size exceeds allocated memory");
        }

        cudaError_t error = cudaMemcpy(deviceMemory_, hostData, size, cudaMemcpyHostToDevice);
        if (error != cudaSuccess) {
            throw std::runtime_error("Failed to copy data to device: " + 
                                   std::string(cudaGetErrorString(error)));
        }
    }

    void copyFromDevice(void* hostData, size_t size) override {
        if (!deviceMemory_) {
            throw std::runtime_error("No device memory allocated");
        }

        if (size > allocatedSize_) {
            throw std::runtime_error("Copy size exceeds allocated memory");
        }

        cudaError_t error = cudaMemcpy(hostData, deviceMemory_, size, cudaMemcpyDeviceToHost);
        if (error != cudaSuccess) {
            throw std::runtime_error("Failed to copy data from device: " + 
                                   std::string(cudaGetErrorString(error)));
        }
    }

    void executeKernel(const std::string& kernelName,
                      const std::vector<void*>& args,
                      const std::vector<size_t>& globalWorkSize,
                      const std::vector<size_t>& localWorkSize) override {
        if (!isInitialized_) {
            throw std::runtime_error("CUDA engine not initialized");
        }

        // Note: This is a simplified version. In practice, you would:
        // 1. Load the kernel from a .cu file
        // 2. Compile it using NVRTC
        // 3. Get kernel function pointer
        // 4. Set up kernel arguments
        // 5. Launch kernel with proper grid/block dimensions

        throw std::runtime_error("Kernel execution not implemented");
    }

    ComputeBackend getBackend() const override {
        return ComputeBackend::CUDA;
    }

    std::string getDeviceName() const override {
        return isInitialized_ ? deviceProps_.name : "Unknown";
    }

    size_t getMaxWorkGroupSize() const override {
        return isInitialized_ ? deviceProps_.maxThreadsPerBlock : 0;
    }

    std::vector<size_t> getMaxWorkItemSizes() const override {
        if (!isInitialized_) return {};
        return {
            static_cast<size_t>(deviceProps_.maxThreadsDim[0]),
            static_cast<size_t>(deviceProps_.maxThreadsDim[1]),
            static_cast<size_t>(deviceProps_.maxThreadsDim[2])
        };
    }

private:
    CudaEngine() : isInitialized_(false), deviceMemory_(nullptr), allocatedSize_(0) {}

    bool isInitialized_;
    void* deviceMemory_;
    size_t allocatedSize_;
    cudaDeviceProp deviceProps_;
};

} // namespace model
} // namespace quant_hub
