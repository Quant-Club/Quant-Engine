#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 200

#include "model/opencl_engine.hpp"
#include "common/logger.hpp"
#include <stdexcept>
#include <fstream>
#include <sstream>

namespace quant_hub {
namespace model {

std::shared_ptr<OpenCLEngine> OpenCLEngine::create() {
    return std::shared_ptr<OpenCLEngine>(new OpenCLEngine());
}

OpenCLEngine::~OpenCLEngine() {
    if (isInitialized()) {
        shutdown();
    }
}

void OpenCLEngine::initialize() {
    if (isInitialized_) return;

    try {
        // Get platforms
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        if (platforms.empty()) {
            throw std::runtime_error("No OpenCL platforms found");
        }

        // Select platform (first available)
        platform_ = platforms[0];

        // Get devices
        std::vector<cl::Device> devices;
        platform_.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        if (devices.empty()) {
            throw std::runtime_error("No OpenCL devices found");
        }

        // Select device (first available)
        device_ = devices[0];

        // Create context
        context_ = cl::Context(device_);

        // Create command queue
        queue_ = cl::CommandQueue(context_, device_);

        isInitialized_ = true;
        LOG_INFO("OpenCL engine initialized on device: ", device_.getInfo<CL_DEVICE_NAME>());

    } catch (const cl::Error& e) {
        LOG_ERROR("OpenCL initialization failed: ", e.what(), " (", e.err(), ")");
        throw;
    }
}

void OpenCLEngine::shutdown() {
    if (!isInitialized_) return;

    try {
        for (auto& buffer : buffers_) {
            buffer.second = cl::Buffer();
        }
        buffers_.clear();
        kernels_.clear();
        queue_ = cl::CommandQueue();
        context_ = cl::Context();
        device_ = cl::Device();
        platform_ = cl::Platform();

        isInitialized_ = false;
        LOG_INFO("OpenCL engine shutdown complete");

    } catch (const cl::Error& e) {
        LOG_ERROR("OpenCL shutdown failed: ", e.what(), " (", e.err(), ")");
        throw;
    }
}

void* OpenCLEngine::allocateMemory(size_t size) {
    if (!isInitialized_) {
        throw std::runtime_error("OpenCL engine not initialized");
    }

    try {
        cl::Buffer buffer = cl::Buffer(context_, CL_MEM_READ_WRITE, size);
        void* ptr = new char[1]; // Dummy pointer for tracking
        buffers_[ptr] = buffer;
        return ptr;

    } catch (const cl::Error& e) {
        LOG_ERROR("OpenCL memory allocation failed: ", e.what(), " (", e.err(), ")");
        throw;
    }
}

void OpenCLEngine::freeMemory(void* ptr) {
    auto it = buffers_.find(ptr);
    if (it != buffers_.end()) {
        it->second = cl::Buffer();
        buffers_.erase(it);
        delete[] static_cast<char*>(ptr);
    }
}

void OpenCLEngine::copyToDevice(void* dst, const void* src, size_t size) {
    auto it = buffers_.find(dst);
    if (it == buffers_.end()) {
        throw std::runtime_error("Invalid device pointer");
    }

    try {
        queue_.enqueueWriteBuffer(it->second, CL_TRUE, 0, size, src);
    } catch (const cl::Error& e) {
        LOG_ERROR("OpenCL data transfer to device failed: ", e.what(), " (", e.err(), ")");
        throw;
    }
}

void OpenCLEngine::copyFromDevice(void* dst, const void* src, size_t size) {
    auto it = buffers_.find(const_cast<void*>(src));
    if (it == buffers_.end()) {
        throw std::runtime_error("Invalid device pointer");
    }

    try {
        queue_.enqueueReadBuffer(it->second, CL_TRUE, 0, size, dst);
    } catch (const cl::Error& e) {
        LOG_ERROR("OpenCL data transfer from device failed: ", e.what(), " (", e.err(), ")");
        throw;
    }
}

void OpenCLEngine::loadKernel(const std::string& name, const std::string& source) {
    try {
        cl::Program program(context_, source);
        program.build({device_});
        kernels_[name] = cl::Kernel(program, name.c_str());
    } catch (const cl::Error& e) {
        std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device_);
        LOG_ERROR("OpenCL kernel build failed: ", log);
        throw;
    }
}

void OpenCLEngine::executeKernel(const std::string& name, 
                               const std::vector<void*>& args,
                               const std::vector<size_t>& globalWorkSize) {
    auto it = kernels_.find(name);
    if (it == kernels_.end()) {
        throw std::runtime_error("Kernel not found: " + name);
    }

    try {
        cl::Kernel& kernel = it->second;

        // Set kernel arguments
        for (size_t i = 0; i < args.size(); ++i) {
            auto bufferIt = buffers_.find(args[i]);
            if (bufferIt == buffers_.end()) {
                throw std::runtime_error("Invalid kernel argument pointer");
            }
            kernel.setArg(i, bufferIt->second);
        }

        // Execute kernel
        cl::NDRange global(globalWorkSize[0], globalWorkSize[1], globalWorkSize[2]);
        queue_.enqueueNDRangeKernel(kernel, cl::NullRange, global);
        queue_.finish();

    } catch (const cl::Error& e) {
        LOG_ERROR("OpenCL kernel execution failed: ", e.what(), " (", e.err(), ")");
        throw;
    }
}

} // namespace model
} // namespace quant_hub
