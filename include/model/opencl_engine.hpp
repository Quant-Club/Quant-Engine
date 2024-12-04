#pragma once

#include <string>
#include <vector>
#include <memory>
#include <CL/cl.hpp>
#include "compute_engine.hpp"

namespace quant_hub {
namespace model {

class OpenCLEngine : public ComputeEngine {
public:
    static std::shared_ptr<OpenCLEngine> create();
    ~OpenCLEngine() override;

    // Initialization
    void initialize() override;
    bool isInitialized() const override;
    void shutdown() override;

    // Memory Management
    void allocateMemory(size_t size) override;
    void freeMemory() override;
    void copyToDevice(const void* hostData, size_t size) override;
    void copyFromDevice(void* hostData, size_t size) override;

    // Kernel Management
    void executeKernel(const std::string& kernelName,
                      const std::vector<void*>& args,
                      const std::vector<size_t>& globalWorkSize,
                      const std::vector<size_t>& localWorkSize) override;

    ComputeBackend getBackend() const override;
    std::string getDeviceName() const override;
    size_t getMaxWorkGroupSize() const override;
    std::vector<size_t> getMaxWorkItemSizes() const override;

private:
    OpenCLEngine() : isInitialized_(false), allocatedSize_(0) {}
    
    bool isInitialized_;
    size_t allocatedSize_;

    cl::Platform platform_;
    cl::Device device_;
    cl::Context context_;
    cl::CommandQueue queue_;
    cl::Buffer buffer_;
    std::map<std::string, cl::Kernel> kernels_;
};

} // namespace model
} // namespace quant_hub
