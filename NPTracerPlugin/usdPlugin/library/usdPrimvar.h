#pragma once

#include "usdMath.h"

#include <NPTracerRenderer/utils.h>

#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/imaging/hd/meshUtil.h>

PXR_NAMESPACE_OPEN_SCOPE

extern const std::array<TfToken, 3> gUVTokensArray;  // calling TfToken constructor is expensive

// NOTE: use a regular enum for easier traversal and indexing
enum PrimvarType : uint8_t
{
    POSITION,
    NORMAL,
    COLOR,
    UV,
    _COUNT
};

class PrimvarPayloadBase
{
protected:
    size_t sourceSize = SIZE_MAX;

public:
    HdPrimvarDescriptor desc{};
    bool isDirty = false;

    virtual void FillDefault(size_t n) = 0;  // fill both source and processed with default elem
    virtual void FillConstant(size_t n) = 0;  // fill processed with first elem ofsource

    virtual void SetSource(VtValue& value) = 0;
    virtual const VtValue& GetSource() const = 0;
    virtual void SetProcessed(VtValue& value) = 0;

    virtual void Prepare(size_t n) = 0;  // prepare payload object state for write
    virtual void UnsafeWrite(uint32_t srcIdx, uint32_t dstIdx) = 0;  // NOTE: safe but unsafe!
    virtual void Cooldown() = 0;  // reset payload object state after write

    virtual ~PrimvarPayloadBase() = default;
};

template<typename T>
class PrimvarPayload : public PrimvarPayloadBase
{
private:
    VtValue source = VtValue(VtArray<T>());  // initialize with empty array for ease-of-use
    VtValue processed = VtValue(VtArray<T>());
    T defaultElement{};

    VtArray<T> runtimeSource{};  // copy of stored values only used during `Process-Write` loop
    VtArray<T> runtimeProcessed{};  // copy of stored values only used during `Process-Write` loop

public:
    explicit inline PrimvarPayload(const T& defaultElem) : defaultElement(defaultElem) {}

    inline void FillDefault(size_t n) override
    {
        if (!source.IsHolding<VtArray<T>>()) TF_FATAL_CODING_ERROR("Source corrupted.");
        source.UncheckedMutate<VtArray<T>>([&](VtArray<T>& src) { src.resize(n, defaultElement); });
        sourceSize = n;
        processed = VtValue(source);  // make processed a deep-copy of source
    }

    inline void FillConstant(size_t n) override
    {
        // accept any source so long as non-empty
        TF_DEV_AXIOM(source.IsHolding<VtArray<T>>() && source.GetArraySize() > 0);
        if (!processed.IsHolding<VtArray<T>>()) TF_FATAL_CODING_ERROR("Processed corrupted.");
        const T elem = source.UncheckedGet<VtArray<T>>()[0];
        processed.UncheckedMutate<VtArray<T>>([&](VtArray<T>& proc) { proc.resize(n, elem); });
    }

    inline void SetSource(VtValue& value) override
    {
        // allow as long as holding the right type
        if (value.IsHolding<VtArray<T>>()) source.UncheckedSwap(value);
        else source.UncheckedRemove<VtArray<T>>();
        sourceSize = source.GetArraySize();  // sets 0 if value is empty
    }

    inline const VtValue& GetSource() const override
    {
        return source;  // return const reference
    }

    inline const VtArray<T>& GetProcessedArray() const
    {
        return processed.UncheckedGet<VtArray<T>>();
    }

    inline void SetProcessed(VtValue& value) override
    {
        // allow as long as holding the right type
        if (value.IsHolding<VtArray<T>>()) processed.UncheckedSwap(value);
        else processed.UncheckedRemove<VtArray<T>>();
    }

    inline void Prepare(size_t n) override
    {
        if (!source.IsHolding<VtArray<T>>() || source.GetArraySize() != sourceSize)
            TF_FATAL_CODING_ERROR("Set or default fill source value of primvar before `Prepare");
        if (sourceSize == 0) TF_FATAL_CODING_ERROR("An extra check to default fill is needed.");

        runtimeSource = source.UncheckedGet<VtArray<T>>();  // copy unavoidable here

        if (!processed.IsHolding<VtArray<T>>()) TF_FATAL_CODING_ERROR("Processed corrupted");

        runtimeProcessed = processed.UncheckedGet<VtArray<T>>();
        runtimeProcessed.resize(n, defaultElement);  // reserve space for write
    }

    // we will make quite a few unsafe assumptions here to keep write as fast as possible
    // don't say we didn't warn you :D
    inline void UnsafeWrite(uint32_t srcIdx, uint32_t dstIdx) override
    {
        // this write step is a design choice.
        // other options are to write default value if out of bounds or write value of last index
        runtimeProcessed[dstIdx] = (srcIdx < sourceSize) ? runtimeSource[srcIdx] : defaultElement;
    }

    inline void Cooldown() override
    {
        source = VtValue::Take<VtArray<T>>(runtimeSource);
        processed = VtValue::Take<VtArray<T>>(runtimeProcessed);
        runtimeSource = {};  // make these variables explictly invalid again
        runtimeProcessed = {};
    }
};

// template types
using IsPrimvarDescFn = bool (*)(const HdPrimvarDescriptor& desc);
using IsPrimvarDirtyFn = bool (*)(const HdDirtyBits* dirtyBits, const SdfPath& id);
using ProcessPrimvarsFn = void (*)(const HdMeshUtil& meshUtil, const VtU32Array& triIndices,
                                   const VtIntArray& primitiveParams,
                                   const std::vector<PrimvarPayloadBase*>& pPayloads);

using PrimvarMap = std::unordered_map<PrimvarType, UPTR<PrimvarPayloadBase>>;

// static templated helper functions
template<typename T>
PrimvarPayload<T>* GetPayload(const PrimvarMap& map, PrimvarType type)
{
    return static_cast<PrimvarPayload<T>*>(map.at(type).get());
}

bool IsPositionPrimvarDesc(const HdPrimvarDescriptor& desc);
bool IsNormalPrimvarDesc(const HdPrimvarDescriptor& desc);
bool IsColorPrimvarDesc(const HdPrimvarDescriptor& desc);
bool IsUVPrimvarDesc(const HdPrimvarDescriptor& desc);

bool IsPositionPrimvarDirty(const HdDirtyBits* dirtyBits, const SdfPath& id);
bool IsNormalPrimvarDirty(const HdDirtyBits* dirtyBits, const SdfPath& id);
bool IsColorPrimvarDirty(const HdDirtyBits* dirtyBits, const SdfPath& id);
bool IsUVPrimvarDirty(const HdDirtyBits* dirtyBits, const SdfPath& id);

void FillMissingPrimvarGfVec2f(size_t count, PrimvarPayloadBase* pPayload);
void FillMissingPrimvarGfVec3f(size_t count, PrimvarPayloadBase* pPayload);

void ProcessPrimvarsConstant(const HdMeshUtil& meshUtil, const VtU32Array& indices,
                             const VtIntArray& primitiveParams,
                             const std::vector<PrimvarPayloadBase*>& pPayloads);

void ProcessPrimvarsUniform(const HdMeshUtil& meshUtil, const VtU32Array& indices,
                            const VtIntArray& primitiveParams,
                            const std::vector<PrimvarPayloadBase*>& pPayloads);

void ProcessPrimvarsVertex(
    const HdMeshUtil& meshUtil, const VtU32Array& indices, const VtIntArray& primitiveParams,
    const std::vector<PrimvarPayloadBase*>& pPayloads);  // also used for `Varying`

void ProcessPrimvarsFaceVarying(const HdMeshUtil& meshUtil, const VtU32Array& indices,
                                const VtIntArray& primitiveParams,
                                const std::vector<PrimvarPayloadBase*>& pPayloads);

// compile-time function tables
// NOTE: these need to match the ordering in `PrimvarType` exactly
inline constexpr IsPrimvarDescFn IS_PRIMVAR_DESC_FN_TABLE[] = {
    &IsPositionPrimvarDesc,  // POSITION
    &IsNormalPrimvarDesc,  // NORMAL
    &IsColorPrimvarDesc,  // COLOR
    &IsUVPrimvarDesc  // UV
};

inline constexpr IsPrimvarDirtyFn IS_PRIMVAR_DIRTY_FN_TABLE[] = {
    &IsPositionPrimvarDirty,  // POSITION
    &IsNormalPrimvarDirty,  // NORMAL
    &IsColorPrimvarDirty,  // COLOR
    &IsUVPrimvarDirty  // UV
};

inline constexpr ProcessPrimvarsFn PROCESS_PRIMVARS_FN_TABLE[] = {
    &ProcessPrimvarsConstant,  // HdInterpolationConstant
    &ProcessPrimvarsUniform,  // HdInterpolationUniform
    &ProcessPrimvarsVertex,  // HdInterpolationVarying
    &ProcessPrimvarsVertex,  // HdInterpolationVertex
    &ProcessPrimvarsFaceVarying,  // HdInterpolationFaceVarying
    nullptr,  // HdInterpolationInstance
};
PXR_NAMESPACE_CLOSE_SCOPE
