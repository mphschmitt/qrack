//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano and the Qrack contributors 2017-2021. All rights reserved.
//
// This is a multithreaded, universal quantum register simulation, allowing
// (nonphysical) register cloning and direct measurement of probability and
// phase, to leverage what advantages classical emulation of qubits can have.
//
// Licensed under the GNU Lesser General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/lgpl-3.0.en.html
// for details.

#pragma once

#define _USE_MATH_DEFINES

#include "config.h"

#if !ENABLE_OPENCL
#error OpenCL has not been enabled
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <direct.h>
#endif

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(__APPLE__)
#define CL_SILENCE_DEPRECATION
#include <OpenCL/cl.hpp>
#elif defined(_WIN32) || ENABLE_SNUCL
#include <CL/cl.hpp>
#else
#include <CL/cl2.hpp>
#endif

namespace Qrack {

class OCLDeviceCall;

class OCLDeviceContext;

typedef std::shared_ptr<OCLDeviceContext> DeviceContextPtr;
typedef std::shared_ptr<std::vector<cl::Event>> EventVecPtr;

enum OCLAPI {
    OCL_API_UNKNOWN = 0,
    OCL_API_APPLY2X2,
    OCL_API_APPLY2X2_SINGLE,
    OCL_API_APPLY2X2_NORM_SINGLE,
    OCL_API_APPLY2X2_DOUBLE,
    OCL_API_APPLY2X2_WIDE,
    OCL_API_APPLY2X2_SINGLE_WIDE,
    OCL_API_APPLY2X2_NORM_SINGLE_WIDE,
    OCL_API_APPLY2X2_DOUBLE_WIDE,
    OCL_API_PHASE_SINGLE,
    OCL_API_PHASE_SINGLE_WIDE,
    OCL_API_INVERT_SINGLE,
    OCL_API_INVERT_SINGLE_WIDE,
    OCL_API_UNIFORMLYCONTROLLED,
    OCL_API_UNIFORMPARITYRZ,
    OCL_API_UNIFORMPARITYRZ_NORM,
    OCL_API_CUNIFORMPARITYRZ,
    OCL_API_COMPOSE,
    OCL_API_COMPOSE_WIDE,
    OCL_API_COMPOSE_MID,
    OCL_API_DECOMPOSEPROB,
    OCL_API_DECOMPOSEAMP,
    OCL_API_DISPOSEPROB,
    OCL_API_DISPOSE,
    OCL_API_PROB,
    OCL_API_PROBREG,
    OCL_API_PROBREGALL,
    OCL_API_PROBMASK,
    OCL_API_PROBMASKALL,
    OCL_API_PROBPARITY,
    OCL_API_FORCEMPARITY,
    OCL_API_EXPPERM,
    OCL_API_X_SINGLE,
    OCL_API_X_SINGLE_WIDE,
    OCL_API_X_MASK,
    OCL_API_Z_SINGLE,
    OCL_API_Z_SINGLE_WIDE,
    OCL_API_PHASE_PARITY,
    OCL_API_ROL,
#if ENABLE_ALU
    OCL_API_INC,
    OCL_API_CINC,
    OCL_API_INCDECC,
    OCL_API_INCS,
    OCL_API_INCDECSC_1,
    OCL_API_INCDECSC_2,
#if ENABLE_BCD
    OCL_API_INCBCD,
    OCL_API_INCDECBCDC,
#endif
    OCL_API_MUL,
    OCL_API_DIV,
    OCL_API_MULMODN_OUT,
    OCL_API_IMULMODN_OUT,
    OCL_API_POWMODN_OUT,
    OCL_API_CMUL,
    OCL_API_CDIV,
    OCL_API_CMULMODN_OUT,
    OCL_API_CIMULMODN_OUT,
    OCL_API_CPOWMODN_OUT,
    OCL_API_FULLADD,
    OCL_API_IFULLADD,
    OCL_API_INDEXEDLDA,
    OCL_API_INDEXEDADC,
    OCL_API_INDEXEDSBC,
    OCL_API_HASH,
    OCL_API_CPHASEFLIPIFLESS,
    OCL_API_PHASEFLIPIFLESS,
#endif
    OCL_API_APPROXCOMPARE,
    OCL_API_NORMALIZE,
    OCL_API_NORMALIZE_WIDE,
    OCL_API_UPDATENORM,
    OCL_API_APPLYM,
    OCL_API_APPLYMREG,
    OCL_API_CLEARBUFFER,
    OCL_API_SHUFFLEBUFFERS
};

struct OCLKernelHandle {
    OCLAPI oclapi;
    std::string kernelname;

    OCLKernelHandle(OCLAPI o, std::string kn)
        : oclapi(o)
        , kernelname(kn)
    {
    }
};

class OCLDeviceCall {
protected:
    std::lock_guard<std::mutex> guard;

public:
    // A cl::Kernel is unique object which should always be taken by reference, or the OCLDeviceContext will lose
    // ownership.
    cl::Kernel& call;
    OCLDeviceCall(const OCLDeviceCall&);

protected:
    OCLDeviceCall(std::mutex& m, cl::Kernel& c)
        : guard(m)
        , call(c)
    {
    }

    friend class OCLDeviceContext;

private:
    OCLDeviceCall& operator=(const OCLDeviceCall&) = delete;
};

class OCLDeviceContext {
public:
    cl::Platform platform;
    cl::Device device;
    cl::Context context;
    int context_id;
    int device_id;
    cl::CommandQueue queue;
    EventVecPtr wait_events;

protected:
    std::mutex waitEventsMutex;
    std::map<OCLAPI, cl::Kernel> calls;
    std::map<OCLAPI, std::unique_ptr<std::mutex>> mutexes;

private:
    const size_t procElemCount = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
    const size_t maxWorkItems = device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>()[0];
    const size_t maxAlloc = device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
    const size_t globalSize = device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
    size_t preferredSizeMultiple;
    size_t preferredConcurrency;

public:
    OCLDeviceContext(cl::Platform& p, cl::Device& d, cl::Context& c, int dev_id, int cntxt_id)
        : platform(p)
        , device(d)
        , context(c)
        , context_id(cntxt_id)
        , device_id(dev_id)
        , preferredSizeMultiple(0)
        , preferredConcurrency(0)
    {
        cl_int error;
        queue = cl::CommandQueue(context, d, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &error);
        if (error != CL_SUCCESS) {
            queue = cl::CommandQueue(context, d);
        }

        wait_events =
            std::shared_ptr<std::vector<cl::Event>>(new std::vector<cl::Event>(), [](std::vector<cl::Event>* vec) {
                vec->clear();
                delete vec;
            });
    }

    OCLDeviceCall Reserve(OCLAPI call) { return OCLDeviceCall(*(mutexes[call]), calls[call]); }

    EventVecPtr ResetWaitEvents()
    {
        std::lock_guard<std::mutex> guard(waitEventsMutex);
        EventVecPtr waitVec = std::move(wait_events);
        wait_events =
            std::shared_ptr<std::vector<cl::Event>>(new std::vector<cl::Event>(), [](std::vector<cl::Event>* vec) {
                vec->clear();
                delete vec;
            });
        return waitVec;
    }

    void LockWaitEvents() { waitEventsMutex.lock(); }

    void UnlockWaitEvents() { waitEventsMutex.unlock(); }

    void WaitOnAllEvents()
    {
        std::lock_guard<std::mutex> guard(waitEventsMutex);
        if ((wait_events.get())->size()) {
            cl::Event::waitForEvents((const std::vector<cl::Event>&)*(wait_events.get()));
            wait_events->clear();
        }
    }

    size_t GetPreferredSizeMultiple()
    {
        return preferredSizeMultiple
            ? preferredSizeMultiple
            : preferredSizeMultiple =
                  calls[OCL_API_APPLY2X2_NORM_SINGLE].getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(
                      device);
    }

    size_t GetPreferredConcurrency()
    {
        if (preferredConcurrency) {
            return preferredConcurrency;
        }

        int hybridOffset = 2U;
#if ENABLE_ENV_VARS
        if (getenv("QRACK_GPU_OFFSET_QB")) {
            hybridOffset = std::stoi(std::string(getenv("QRACK_GPU_OFFSET_QB")));
        }
#endif

        const size_t pc = procElemCount * GetPreferredSizeMultiple();
        preferredConcurrency = 1U;
        while (preferredConcurrency < pc) {
            preferredConcurrency <<= 1U;
        }
        preferredConcurrency =
            hybridOffset > 0 ? (preferredConcurrency << hybridOffset) : (preferredConcurrency >> hybridOffset);
        if (preferredConcurrency < 1U) {
            preferredConcurrency = 1U;
        }

        return preferredConcurrency;
    }

    size_t GetProcElementCount() { return procElemCount; }
    size_t GetMaxWorkItems() { return maxWorkItems; }
    size_t GetMaxAlloc() { return maxAlloc; }
    size_t GetGlobalSize() { return globalSize; }

    friend class OCLEngine;
};

struct InitOClResult {
    std::vector<DeviceContextPtr> all_dev_contexts;
    DeviceContextPtr default_dev_context;

    InitOClResult()
        : all_dev_contexts()
        , default_dev_context(NULL)
    {
        // Intentionally left blank
    }

    InitOClResult(std::vector<DeviceContextPtr> adc, DeviceContextPtr ddc)
        : all_dev_contexts(adc)
        , default_dev_context(ddc)
    {
        // Intentionally left blank
    }
};

/** "Qrack::OCLEngine" manages the single OpenCL context. */
class OCLEngine {
public:
    // See https://stackoverflow.com/questions/1008019/c-singleton-design-pattern
    /// Get a pointer to the Instance of the singleton. (The instance will be instantiated, if it does not exist yet.)
    static OCLEngine& Instance()
    {
        static OCLEngine instance;
        return instance;
    }
    static std::string GetDefaultBinaryPath()
    {
#if ENABLE_ENV_VARS
        if (getenv("QRACK_OCL_PATH")) {
            std::string toRet = std::string(getenv("QRACK_OCL_PATH"));
            if ((toRet.back() != '/') && (toRet.back() != '\\')) {
#if defined(_WIN32) && !defined(__CYGWIN__)
                toRet += "\\";
#else
                toRet += "/";
#endif
            }
            return toRet;
        }
#endif
#if defined(_WIN32) && !defined(__CYGWIN__)
        return std::string(getenv("HOMEDRIVE") ? getenv("HOMEDRIVE") : "") +
            std::string(getenv("HOMEPATH") ? getenv("HOMEPATH") : "") + "\\.qrack\\";
#else
        return std::string(getenv("HOME") ? getenv("HOME") : "") + "/.qrack/";
#endif
    }
    /// Initialize the OCL environment, with the option to save the generated binaries. Binaries will be saved/loaded
    /// from the folder path "home". This returns a Qrack::OCLInitResult object which should be passed to
    /// SetDeviceContextPtrVector().
    static InitOClResult InitOCL(bool buildFromSource = false, bool saveBinaries = false, std::string home = "*");

    /// Get a pointer one of the available OpenCL contexts, by its index in the list of all contexts.
    DeviceContextPtr GetDeviceContextPtr(const int& dev = -1);
    /// Get the list of all available devices (and their supporting objects).
    std::vector<DeviceContextPtr> GetDeviceContextPtrVector();
    /** Set the list of DeviceContextPtr object available for use. If one takes the result of
     * GetDeviceContextPtrVector(), trims items from it, and sets it with this method, (at initialization, before any
     * QEngine objects depend on them,) all resources associated with the removed items are freed.
     */
    void SetDeviceContextPtrVector(std::vector<DeviceContextPtr> vec, DeviceContextPtr dcp = nullptr);
    /// Get the count of devices in the current list.
    int GetDeviceCount() { return all_device_contexts.size(); }
    /// Get default device ID.
    size_t GetDefaultDeviceID() { return default_device_context->device_id; }
    /// Pick a default device, for QEngineOCL instances that don't specify a preferred device.
    void SetDefaultDeviceContext(DeviceContextPtr dcp);
    /// Get default location for precompiled binaries:
    size_t GetMaxActiveAllocSize() { return maxActiveAllocSize; }
    size_t GetActiveAllocSize(const int& dev)
    {
        return (dev < 0) ? activeAllocSizes[GetDefaultDeviceID()] : activeAllocSizes[(size_t)dev];
    }
    size_t AddToActiveAllocSize(const int& dev, size_t size)
    {
        if (dev < -1) {
            throw std::runtime_error("Invalid device selection: " + std::to_string(dev));
        }
        size_t lDev = (dev == -1) ? GetDefaultDeviceID() : dev;

        if (size == 0) {
            return activeAllocSizes[lDev];
        }

        std::lock_guard<std::mutex> lock(allocMutex);
        activeAllocSizes[lDev] += size;

        return activeAllocSizes[lDev];
    }
    size_t SubtractFromActiveAllocSize(const int& dev, size_t size)
    {
        if (dev < -1) {
            throw std::runtime_error("Invalid device selection: " + std::to_string(dev));
        }
        int lDev = (dev == -1) ? GetDefaultDeviceID() : dev;

        if (size == 0) {
            return activeAllocSizes[lDev];
        }

        std::lock_guard<std::mutex> lock(allocMutex);
        if (size < activeAllocSizes[lDev]) {
            activeAllocSizes[lDev] -= size;
        } else {
            activeAllocSizes[lDev] = 0;
        }
        return activeAllocSizes[lDev];
    }
    void ResetActiveAllocSize(const int& dev)
    {
        int lDev = (dev == -1) ? GetDefaultDeviceID() : dev;
        std::lock_guard<std::mutex> lock(allocMutex);
        // User code should catch std::bad_alloc and reset:
        activeAllocSizes[lDev] = 0;
    }

    OCLEngine(OCLEngine const&) = delete;
    void operator=(OCLEngine const&) = delete;

private:
    static const std::vector<OCLKernelHandle> kernelHandles;
    static const std::string binary_file_prefix;
    static const std::string binary_file_ext;

    std::vector<size_t> activeAllocSizes;
    size_t maxActiveAllocSize;
    std::mutex allocMutex;
    std::vector<DeviceContextPtr> all_device_contexts;
    DeviceContextPtr default_device_context;

    OCLEngine(); // Private so that it can  not be called

    /// Make the program, from either source or binary
    static cl::Program MakeProgram(bool buildFromSource, cl::Program::Sources sources, std::string path,
        std::shared_ptr<OCLDeviceContext> devCntxt);
    /// Save the program binary:
    static void SaveBinary(cl::Program program, std::string path, std::string fileName);

    unsigned long PowerOf2LessThan(unsigned long number);
};

} // namespace Qrack
