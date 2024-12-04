#pragma once

#include <vector>
#include <memory>
#include "common/types.hpp"

namespace quant_hub {
namespace model {

enum class ComputeBackend {
    CPU,
    CUDA,
    OpenCL
};

class ComputeEngine {
public:
    static std::shared_ptr<ComputeEngine> create(ComputeBackend backend);
    virtual ~ComputeEngine() = default;

    // Initialization
    virtual void initialize() = 0;
    virtual bool isInitialized() const = 0;
    virtual void shutdown() = 0;

    // Memory management
    virtual void allocateMemory(size_t size) = 0;
    virtual void freeMemory() = 0;
    virtual void copyToDevice(const void* hostData, size_t size) = 0;
    virtual void copyFromDevice(void* hostData, size_t size) = 0;

    // Computation
    virtual void executeKernel(const std::string& kernelName,
                             const std::vector<void*>& args,
                             const std::vector<size_t>& globalWorkSize,
                             const std::vector<size_t>& localWorkSize) = 0;

    // Device information
    virtual ComputeBackend getBackend() const = 0;
    virtual std::string getDeviceName() const = 0;
    virtual size_t getMaxWorkGroupSize() const = 0;
    virtual std::vector<size_t> getMaxWorkItemSizes() const = 0;

protected:
    ComputeEngine() = default;
};

} // namespace model
} // namespace quant_hub
