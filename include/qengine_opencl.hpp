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

#include "common/qrack_types.hpp"

#if !ENABLE_OPENCL
#error OpenCL has not been enabled
#endif

#include "common/oclengine.hpp"
#include "qengine.hpp"

#include <list>
#include <mutex>

#define BCI_ARG_LEN 10
#define CMPLX_NORM_LEN 6
#define REAL_ARG_LEN 2

namespace Qrack {

enum SPECIAL_2X2 { NONE = 0, PAULIX, PAULIZ, INVERT, PHASE };

class bad_alloc : virtual public std::bad_alloc {
private:
    std::string m;

public:
    bad_alloc(std::string message)
        : m(message)
    {
        // Intentionally left blank.
    }

    virtual const char* what() const noexcept { return m.c_str(); }
};

typedef std::shared_ptr<cl::Buffer> BufferPtr;

class QEngineOCL;

typedef std::shared_ptr<QEngineOCL> QEngineOCLPtr;

struct QueueItem {
    OCLAPI api_call;
    size_t workItemCount;
    size_t localGroupSize;
    size_t deallocSize;
    std::vector<BufferPtr> buffers;
    size_t localBuffSize;
    bool isSetDoNorm;
    bool isSetRunningNorm;
    bool doNorm;
    real1 runningNorm;
    std::mutex* otherMutex;

    QueueItem(OCLAPI ac, size_t wic, size_t lgs, size_t ds, std::vector<BufferPtr> b, size_t lbs)
        : api_call(ac)
        , workItemCount(wic)
        , localGroupSize(lgs)
        , deallocSize(ds)
        , buffers(b)
        , localBuffSize(lbs)
        , isSetDoNorm(false)
        , isSetRunningNorm(false)
        , doNorm(false)
        , runningNorm(ONE_R1)
        , otherMutex(NULL)
    {
    }

    QueueItem(bool doNrm)
        : api_call()
        , workItemCount(0)
        , localGroupSize(0)
        , deallocSize(0)
        , buffers()
        , localBuffSize(0)
        , isSetDoNorm(true)
        , isSetRunningNorm(false)
        , doNorm(doNrm)
        , runningNorm(ONE_R1)
        , otherMutex(NULL)
    {
    }

    QueueItem(real1_f runningNrm)
        : api_call()
        , workItemCount(0)
        , localGroupSize(0)
        , deallocSize(0)
        , buffers()
        , localBuffSize(0)
        , isSetDoNorm(false)
        , isSetRunningNorm(true)
        , doNorm(false)
        , runningNorm(runningNrm)
        , otherMutex(NULL)
    {
    }
};

struct PoolItem {
    BufferPtr cmplxBuffer;
    BufferPtr realBuffer;
    BufferPtr ulongBuffer;

    std::shared_ptr<real1> probArray;
    std::shared_ptr<real1> angleArray;
    complex* otherStateVec;

    virtual BufferPtr MakeBuffer(const cl::Context& context, cl_mem_flags flags, size_t size, void* host_ptr = NULL)
    {
        cl_int error;
        BufferPtr toRet = std::make_shared<cl::Buffer>(context, flags, size, host_ptr, &error);
        if (error != CL_SUCCESS) {
            if (error == CL_MEM_OBJECT_ALLOCATION_FAILURE) {
                throw bad_alloc("CL_MEM_OBJECT_ALLOCATION_FAILURE in PoolItem::MakeBuffer()");
            }
            if (error == CL_OUT_OF_HOST_MEMORY) {
                throw bad_alloc("CL_OUT_OF_HOST_MEMORY in PoolItem::MakeBuffer()");
            }
            if (error == CL_INVALID_BUFFER_SIZE) {
                throw bad_alloc("CL_INVALID_BUFFER_SIZE in PoolItem::MakeBuffer()");
            }
            throw std::runtime_error("OpenCL error code on buffer allocation attempt: " + std::to_string(error));
        }

        return toRet;
    }

    PoolItem(cl::Context& context)
        : probArray(NULL)
        , angleArray(NULL)
        , otherStateVec(NULL)
    {
        cmplxBuffer = MakeBuffer(context, CL_MEM_READ_ONLY, sizeof(complex) * CMPLX_NORM_LEN);
        realBuffer = MakeBuffer(context, CL_MEM_READ_ONLY, sizeof(real1) * REAL_ARG_LEN);
        ulongBuffer = MakeBuffer(context, CL_MEM_READ_ONLY, sizeof(bitCapIntOcl) * BCI_ARG_LEN);
    }

    virtual ~PoolItem() {}
};

typedef std::shared_ptr<PoolItem> PoolItemPtr;

/**
 * OpenCL enhanced QEngineCPU implementation.
 *
 * QEngineOCL exposes asynchronous void-return public methods, wherever possible. While QEngine public methods run on a
 * secondary accelerator, such as a GPU, other code can be executed on the CPU at the same time. If only one (CPU)
 * OpenCL device is available, this engine type is still compatible with most CPUs, and this implementation will still
 * usually give a very significant performance boost over the non-OpenCL QEngineCPU implementation.
 *
 * Each QEngineOCL queues an independent event list of chained asynchronous methods. Multiple QEngineOCL instances may
 * share a single device. Any one QEngineOCL instance (or QEngineCPU instance) is NOT safe to access from multiple
 * threads, but different QEngineOCL instances may be accessed in respective threads. When a public method with a
 * non-void return type is called, (such as Prob() or M() variants,) the engine wait list of OpenCL events will first be
 * finished, then the return value will be calculated based on all public method calls dispatched up to that point.
 * Asynchronous method dispatch is "transparent," in the sense that no explicit consideration for synchronization should
 * be necessary. The programmer benefits from knowing that void-return methods attempt asynchronous execution, but
 * asynchronous methods are always joined, in order of dispatch, before any and all non-void-return methods give their
 * results.
 */
class QEngineOCL : virtual public QEngine {
protected:
    complex* stateVec;
    int deviceID;
    DeviceContextPtr device_context;
    std::vector<EventVecPtr> wait_refs;
    std::list<QueueItem> wait_queue_items;
    std::mutex queue_mutex;
    cl::CommandQueue queue;
    cl::Context context;
    // stateBuffer is allocated as a shared_ptr, because it's the only buffer that will be acted on outside of
    // QEngineOCL itself, specifically by QEngineOCLMulti.
    BufferPtr stateBuffer;
    BufferPtr nrmBuffer;
    BufferPtr powersBuffer;
    std::vector<PoolItemPtr> poolItems;
    std::unique_ptr<real1, void (*)(real1*)> nrmArray;
    size_t nrmGroupCount;
    size_t nrmGroupSize;
    size_t maxWorkItems;
    size_t maxMem;
    size_t maxAlloc;
    size_t totalOclAllocSize;
    size_t preferredConcurrency;
    bool unlockHostMem;
    cl_map_flags lockSyncFlags;
    bool usingHostRam;
    complex permutationAmp;

#if defined(__APPLE__)
    real1* _aligned_nrm_array_alloc(bitCapIntOcl allocSize)
    {
        void* toRet;
        posix_memalign(&toRet, QRACK_ALIGN_SIZE, allocSize);
        return (real1*)toRet;
    }
#endif

public:
    /// 1 / OclMemDenom is the maximum fraction of total OCL device RAM that a single state vector should occupy, by
    /// design of the QEngine.
    static const bitCapIntOcl OclMemDenom = 3U;

    /**
     * Initialize a Qrack::QEngineOCL object. Specify the number of qubits and an initial permutation state.
     * Additionally, optionally specify a pointer to a random generator engine object, a device ID from the list of
     * devices in the OCLEngine singleton, and a boolean that is set to "true" to initialize the state vector of the
     * object to zero norm.
     *
     * "devID" is the index of an OpenCL device in the OCLEngine singleton, to select the device to run this engine on.
     * If "useHostMem" is set false, as by default, the QEngineOCL will attempt to allocate the state vector object
     * only on device memory. If "useHostMem" is set true, general host RAM will be used for the state vector buffers.
     * If the state vector is too large to allocate only on device memory, the QEngineOCL will attempt to fall back to
     * allocating it in general host RAM.
     *
     * \warning "useHostMem" is not conscious of allocation by other QEngineOCL instances on the same device. Attempting
     * to allocate too much device memory across too many QEngineOCL instances, for which each instance would have
     * sufficient device resources on its own, will probably cause the program to crash (and may lead to general system
     * instability). For safety, "useHostMem" can be turned on.
     */

    QEngineOCL(bitLenInt qBitCount, bitCapInt initState, qrack_rand_gen_ptr rgp = nullptr,
        complex phaseFac = CMPLX_DEFAULT_ARG, bool doNorm = false, bool randomGlobalPhase = true,
        bool useHostMem = false, int devID = -1, bool useHardwareRNG = true, bool ignored = false,
        real1_f norm_thresh = REAL1_EPSILON, std::vector<int> ignored2 = {}, bitLenInt ignored4 = 0,
        real1_f ignored3 = FP_NORM_EPSILON_F);

    virtual ~QEngineOCL()
    {
        clDump();
        FreeAll();
    }

    virtual bool IsZeroAmplitude() { return !stateBuffer; }
    virtual real1_f FirstNonzeroPhase()
    {
        if (!stateBuffer) {
            return ZERO_R1_F;
        }

        return QInterface::FirstNonzeroPhase();
    }

    virtual void FreeAll();
    virtual void ZeroAmplitudes();
    virtual void FreeStateVec(complex* sv = NULL);
    virtual void CopyStateVec(QEnginePtr src);

    virtual void GetAmplitudePage(complex* pagePtr, bitCapIntOcl offset, bitCapIntOcl length);
    virtual void SetAmplitudePage(const complex* pagePtr, bitCapIntOcl offset, bitCapIntOcl length);
    virtual void SetAmplitudePage(
        QEnginePtr pageEnginePtr, bitCapIntOcl srcOffset, bitCapIntOcl dstOffset, bitCapIntOcl length);
    virtual void ShuffleBuffers(QEnginePtr engine);
    virtual QEnginePtr CloneEmpty();

    virtual void QueueSetDoNormalize(bool doNorm) { AddQueueItem(QueueItem(doNorm)); }
    virtual void QueueSetRunningNorm(real1_f runningNrm) { AddQueueItem(QueueItem(runningNrm)); }
    virtual void AddQueueItem(const QueueItem& item)
    {
        bool isBase;
        // For lock_guard:
        if (true) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            isBase = (wait_queue_items.size() == 0);
            wait_queue_items.push_back(item);
        }

        if (isBase) {
            DispatchQueue(NULL, CL_COMPLETE);
        }
    }
    virtual void QueueCall(OCLAPI api_call, size_t workItemCount, size_t localGroupSize, std::vector<BufferPtr> args,
        size_t localBuffSize = 0, size_t deallocSize = 0)
    {
        AddQueueItem(QueueItem(api_call, workItemCount, localGroupSize, deallocSize, args, localBuffSize));
    }

    bitCapIntOcl GetMaxSize() { return maxAlloc / sizeof(complex); };

    virtual void SetPermutation(bitCapInt perm, complex phaseFac = CMPLX_DEFAULT_ARG);

    virtual void UniformlyControlledSingleBit(const bitLenInt* controls, bitLenInt controlLen, bitLenInt qubitIndex,
        const complex* mtrxs, const bitCapInt* mtrxSkipPowers, bitLenInt mtrxSkipLen, bitCapInt mtrxSkipValueMask);
    virtual void UniformParityRZ(bitCapInt mask, real1_f angle);
    virtual void CUniformParityRZ(const bitLenInt* controls, bitLenInt controlLen, bitCapInt mask, real1_f angle);

    /* Operations that have an improved implementation. */
    using QEngine::X;
    virtual void X(bitLenInt target);
    using QEngine::Z;
    virtual void Z(bitLenInt target);
    using QEngine::Invert;
    virtual void Invert(complex topRight, complex bottomLeft, bitLenInt qubitIndex);
    using QEngine::Phase;
    virtual void Phase(complex topLeft, complex bottomRight, bitLenInt qubitIndex);

    virtual void XMask(bitCapInt mask)
    {
        if (!mask) {
            return;
        }

        if (!(mask & (mask - ONE_BCI))) {
            X(log2(mask));
            return;
        }

        BitMask((bitCapIntOcl)mask, OCL_API_X_MASK);
    }
    virtual void PhaseParity(real1_f radians, bitCapInt mask)
    {
        if (!mask) {
            return;
        }

        if (!(mask & (mask - ONE_BCI))) {
            complex phaseFac = std::polar(ONE_R1, (real1)(radians / 2));
            Phase(ONE_CMPLX / phaseFac, phaseFac, log2(mask));
            return;
        }

        BitMask((bitCapIntOcl)mask, OCL_API_PHASE_PARITY, radians);
    }

    using QEngine::Compose;
    virtual bitLenInt Compose(QEngineOCLPtr toCopy);
    virtual bitLenInt Compose(QInterfacePtr toCopy) { return Compose(std::dynamic_pointer_cast<QEngineOCL>(toCopy)); }
    virtual bitLenInt Compose(QEngineOCLPtr toCopy, bitLenInt start);
    virtual bitLenInt Compose(QInterfacePtr toCopy, bitLenInt start)
    {
        return Compose(std::dynamic_pointer_cast<QEngineOCL>(toCopy), start);
    }
    using QEngine::Decompose;
    virtual void Decompose(bitLenInt start, QInterfacePtr dest);
    virtual void Dispose(bitLenInt start, bitLenInt length);
    virtual void Dispose(bitLenInt start, bitLenInt length, bitCapInt disposedPerm);

    virtual void ROL(bitLenInt shift, bitLenInt start, bitLenInt length);

#if ENABLE_ALU
    virtual void INC(bitCapInt toAdd, bitLenInt start, bitLenInt length);
    virtual void CINC(
        bitCapInt toAdd, bitLenInt inOutStart, bitLenInt length, const bitLenInt* controls, bitLenInt controlLen);
    virtual void INCS(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
#if ENABLE_BCD
    virtual void INCBCD(bitCapInt toAdd, bitLenInt start, bitLenInt length);
#endif
    virtual void MUL(bitCapInt toMul, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length);
    virtual void DIV(bitCapInt toDiv, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length);
    virtual void MULModNOut(bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length);
    virtual void IMULModNOut(bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length);
    virtual void POWModNOut(bitCapInt base, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length);
    virtual void CMUL(bitCapInt toMul, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length,
        const bitLenInt* controls, bitLenInt controlLen);
    virtual void CDIV(bitCapInt toDiv, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length,
        const bitLenInt* controls, bitLenInt controlLen);
    virtual void CMULModNOut(bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length,
        const bitLenInt* controls, bitLenInt controlLen);
    virtual void CIMULModNOut(bitCapInt toMul, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length,
        const bitLenInt* controls, bitLenInt controlLen);
    virtual void CPOWModNOut(bitCapInt base, bitCapInt modN, bitLenInt inStart, bitLenInt outStart, bitLenInt length,
        const bitLenInt* controls, bitLenInt controlLen);
    virtual void FullAdd(bitLenInt inputBit1, bitLenInt inputBit2, bitLenInt carryInSumOut, bitLenInt carryOut);
    virtual void IFullAdd(bitLenInt inputBit1, bitLenInt inputBit2, bitLenInt carryInSumOut, bitLenInt carryOut);

    virtual bitCapInt IndexedLDA(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
        bitLenInt valueLength, const unsigned char* values, bool resetValue = true);
    virtual bitCapInt IndexedADC(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
        bitLenInt valueLength, bitLenInt carryIndex, const unsigned char* values);
    virtual bitCapInt IndexedSBC(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
        bitLenInt valueLength, bitLenInt carryIndex, const unsigned char* values);
    virtual void Hash(bitLenInt start, bitLenInt length, const unsigned char* values);

    virtual void CPhaseFlipIfLess(bitCapInt greaterPerm, bitLenInt start, bitLenInt length, bitLenInt flagIndex);
    virtual void PhaseFlipIfLess(bitCapInt greaterPerm, bitLenInt start, bitLenInt length);
#endif

    virtual real1_f Prob(bitLenInt qubit);
    virtual real1_f ProbReg(bitLenInt start, bitLenInt length, bitCapInt permutation);
    virtual void ProbRegAll(bitLenInt start, bitLenInt length, real1* probsArray);
    virtual real1_f ProbMask(bitCapInt mask, bitCapInt permutation);
    virtual void ProbMaskAll(bitCapInt mask, real1* probsArray);
    virtual real1_f ProbParity(bitCapInt mask);
    virtual bool ForceMParity(bitCapInt mask, bool result, bool doForce = true);
    virtual real1_f ExpectationBitsAll(const bitLenInt* bits, bitLenInt length, bitCapInt offset = 0);

    virtual void SetDevice(int dID, bool forceReInit = false);
    virtual int64_t GetDevice() { return deviceID; }

    virtual void SetQuantumState(const complex* inputState);
    virtual void GetQuantumState(complex* outputState);
    virtual void GetProbs(real1* outputProbs);
    virtual complex GetAmplitude(bitCapInt perm);
    virtual void SetAmplitude(bitCapInt perm, complex amp);

    virtual real1_f SumSqrDiff(QInterfacePtr toCompare)
    {
        return SumSqrDiff(std::dynamic_pointer_cast<QEngineOCL>(toCompare));
    }
    virtual real1_f SumSqrDiff(QEngineOCLPtr toCompare);

    virtual void NormalizeState(
        real1_f nrm = REAL1_DEFAULT_ARG, real1_f norm_thresh = REAL1_DEFAULT_ARG, real1_f phaseArg = ZERO_R1_F);
    ;
    virtual void UpdateRunningNorm(real1_f norm_thresh = REAL1_DEFAULT_ARG);
    virtual void Finish() { clFinish(); };
    virtual bool isFinished() { return (wait_queue_items.size() == 0); };

    virtual QInterfacePtr Clone();

    void PopQueue(cl_event event, cl_int type);
    void DispatchQueue(cl_event event, cl_int type);

protected:
    virtual void AddAlloc(size_t size)
    {
        size_t currentAlloc = OCLEngine::Instance().AddToActiveAllocSize(deviceID, size);
        if (currentAlloc > OCLEngine::Instance().GetMaxActiveAllocSize()) {
            OCLEngine::Instance().SubtractFromActiveAllocSize(deviceID, size);
            FreeAll();
            throw bad_alloc("VRAM limits exceeded in QEngineOCL::AddAlloc()");
        }
        totalOclAllocSize += size;
    }
    virtual void SubtractAlloc(size_t size)
    {
        OCLEngine::Instance().SubtractFromActiveAllocSize(deviceID, size);
        totalOclAllocSize -= size;
    }

    virtual BufferPtr MakeBuffer(const cl::Context& context, cl_mem_flags flags, size_t size, void* host_ptr = NULL)
    {
        cl_int error;
        BufferPtr toRet = std::make_shared<cl::Buffer>(context, flags, size, host_ptr, &error);
        if (error != CL_SUCCESS) {
            FreeAll();
            if (error == CL_MEM_OBJECT_ALLOCATION_FAILURE) {
                throw bad_alloc("CL_MEM_OBJECT_ALLOCATION_FAILURE in QEngineOCL::MakeBuffer()");
            }
            if (error == CL_OUT_OF_HOST_MEMORY) {
                throw bad_alloc("CL_OUT_OF_HOST_MEMORY in QEngineOCL::MakeBuffer()");
            }
            if (error == CL_INVALID_BUFFER_SIZE) {
                throw bad_alloc("CL_INVALID_BUFFER_SIZE in QEngineOCL::MakeBuffer()");
            }
            throw std::runtime_error("OpenCL error code on buffer allocation attempt: " + std::to_string(error));
        }

        return toRet;
    }

    virtual real1_f GetExpectation(bitLenInt valueStart, bitLenInt valueLength);

    virtual complex* AllocStateVec(bitCapInt elemCount, bool doForceAlloc = false);
    virtual void ResetStateVec(complex* sv);
    virtual void ResetStateBuffer(BufferPtr nStateBuffer);
    virtual BufferPtr MakeStateVecBuffer(complex* nStateVec);
    virtual void ReinitBuffer();

    virtual void Compose(OCLAPI apiCall, bitCapIntOcl* bciArgs, QEngineOCLPtr toCopy);

    void InitOCL(int devID);
    PoolItemPtr GetFreePoolItem();

    real1_f ParSum(real1* toSum, bitCapIntOcl maxI);

    /**
     * Locks synchronization between the state vector buffer and general RAM, so the state vector can be directly read
     * and/or written to.
     *
     * OpenCL buffers, even when allocated on "host" general RAM, are not safe to read from or write to unless "mapped."
     * When mapped, a buffer cannot be used by OpenCL kernels. If the state vector needs to be directly manipulated, it
     * needs to be temporarily mapped, and this can be accomplished with LockSync(). When direct reading from or writing
     * to the state vector is done, before performing other OpenCL operations on it, it must be unmapped with
     * UnlockSync().
     */
    void LockSync(cl_map_flags flags = (CL_MAP_READ | CL_MAP_WRITE));
    /**
     * Unlocks synchronization between the state vector buffer and general RAM, so the state vector can be operated on
     * with OpenCL kernels and operations.
     *
     * OpenCL buffers, even when allocated on "host" general RAM, are not safe to read from or write to unless "mapped."
     * When mapped, a buffer cannot be used by OpenCL kernels. If the state vector needs to be directly manipulated, it
     * needs to be temporarily mapped, and this can be accomplished with LockSync(). When direct reading from or writing
     * to the state vector is done, before performing other OpenCL operations on it, it must be unmapped with
     * UnlockSync().
     */
    void UnlockSync();

    /**
     * Finishes the asynchronous wait event list or queue of OpenCL events.
     *
     * By default (doHard = false) only the wait event list of this engine is finished. If doHard = true, the entire
     * device queue is finished, (which might be shared by other QEngineOCL instances).
     */
    virtual void clFinish(bool doHard = false);

    /**
     * Flushes the OpenCL event queue, and checks for errors.
     */
    virtual void clFlush()
    {
        cl_int error = queue.flush();
        if (error != CL_SUCCESS) {
            FreeAll();
            throw std::runtime_error("Failed to flush queue, error code: " + std::to_string(error));
        }
    }

    /**
     * Dumps the remaining asynchronous wait event list or queue of OpenCL events, for the current queue.
     */
    virtual void clDump();

    size_t FixWorkItemCount(size_t maxI, size_t wic);
    size_t FixGroupSize(size_t wic, size_t gs);

    void DecomposeDispose(bitLenInt start, bitLenInt length, QEngineOCLPtr dest);

    using QEngine::Apply2x2;
    virtual void Apply2x2(bitCapIntOcl offset1, bitCapIntOcl offset2, const complex* mtrx, bitLenInt bitCount,
        const bitCapIntOcl* qPowersSorted, bool doCalcNorm, real1_f norm_thresh = REAL1_DEFAULT_ARG)
    {
        Apply2x2(offset1, offset2, mtrx, bitCount, qPowersSorted, doCalcNorm, SPECIAL_2X2::NONE, norm_thresh);
    }
    virtual void Apply2x2(bitCapIntOcl offset1, bitCapIntOcl offset2, const complex* mtrx, bitLenInt bitCount,
        const bitCapIntOcl* qPowersSorted, bool doCalcNorm, SPECIAL_2X2 special,
        real1_f norm_thresh = REAL1_DEFAULT_ARG);

    virtual void BitMask(bitCapIntOcl mask, OCLAPI api_call, real1_f phase = (real1_f)PI_R1);

    virtual void ApplyM(bitCapInt mask, bool result, complex nrm);
    virtual void ApplyM(bitCapInt mask, bitCapInt result, complex nrm);

    /* Utility functions used by the operations above. */
    void WaitCall(OCLAPI api_call, size_t workItemCount, size_t localGroupSize, std::vector<BufferPtr> args,
        size_t localBuffSize = 0);
    EventVecPtr ResetWaitEvents(bool waitQueue = true);
    void ApplyMx(OCLAPI api_call, bitCapIntOcl* bciArgs, complex nrm);
    real1_f Probx(OCLAPI api_call, bitCapIntOcl* bciArgs);

    void ArithmeticCall(OCLAPI api_call, bitCapIntOcl (&bciArgs)[BCI_ARG_LEN], const unsigned char* values = NULL,
        bitCapIntOcl valuesLength = 0);
    void CArithmeticCall(OCLAPI api_call, bitCapIntOcl (&bciArgs)[BCI_ARG_LEN], bitCapIntOcl* controlPowers,
        bitLenInt controlLen, const unsigned char* values = NULL, bitCapIntOcl valuesLength = 0);
    void ROx(OCLAPI api_call, bitLenInt shift, bitLenInt start, bitLenInt length);

#if ENABLE_ALU
    virtual void INCDECC(bitCapInt toMod, bitLenInt inOutStart, bitLenInt length, bitLenInt carryIndex);
    virtual void INCDECSC(bitCapInt toMod, bitLenInt inOutStart, bitLenInt length, bitLenInt carryIndex);
    virtual void INCDECSC(
        bitCapInt toMod, bitLenInt inOutStart, bitLenInt length, bitLenInt overflowIndex, bitLenInt carryIndex);
#if ENABLE_BCD
    virtual void INCDECBCDC(bitCapInt toMod, bitLenInt inOutStart, bitLenInt length, bitLenInt carryIndex);
#endif

    void INT(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt inOutStart, bitLenInt length);
    void CINT(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt start, bitLenInt length, const bitLenInt* controls,
        bitLenInt controlLen);
    void INTC(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt inOutStart, bitLenInt length, bitLenInt carryIndex);
    void INTS(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt inOutStart, bitLenInt length, bitLenInt overflowIndex);
    void INTSC(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt inOutStart, bitLenInt length, bitLenInt carryIndex);
    void INTSC(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt inOutStart, bitLenInt length, bitLenInt overflowIndex,
        bitLenInt carryIndex);
#if ENABLE_BCD
    void INTBCD(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt inOutStart, bitLenInt length);
    void INTBCDC(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt inOutStart, bitLenInt length, bitLenInt carryIndex);
#endif
    void xMULx(OCLAPI api_call, bitCapIntOcl* bciArgs, BufferPtr controlBuffer);
    void MULx(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length);
    void MULModx(OCLAPI api_call, bitCapIntOcl toMod, bitCapIntOcl modN, bitLenInt inOutStart, bitLenInt carryStart,
        bitLenInt length);
    void CMULx(OCLAPI api_call, bitCapIntOcl toMod, bitLenInt inOutStart, bitLenInt carryStart, bitLenInt length,
        const bitLenInt* controls, bitLenInt controlLen);
    void CMULModx(OCLAPI api_call, bitCapIntOcl toMod, bitCapIntOcl modN, bitLenInt inOutStart, bitLenInt carryStart,
        bitLenInt length, const bitLenInt* controls, bitLenInt controlLen);
    void FullAdx(
        bitLenInt inputBit1, bitLenInt inputBit2, bitLenInt carryInSumOut, bitLenInt carryOut, OCLAPI api_call);
    void PhaseFlipX(OCLAPI api_call, bitCapIntOcl* bciArgs);

    bitCapIntOcl OpIndexed(OCLAPI api_call, bitCapIntOcl carryIn, bitLenInt indexStart, bitLenInt indexLength,
        bitLenInt valueStart, bitLenInt valueLength, bitLenInt carryIndex, const unsigned char* values);
#endif

    void ClearBuffer(BufferPtr buff, bitCapIntOcl offset, bitCapIntOcl size);
};

} // namespace Qrack
