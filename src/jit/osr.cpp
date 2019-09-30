// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#include "osr.h"
#include "corjit.h"

//------------------------------------------------------------------------
// Allocate: ask the VM to allocate a new PatchpointInfo structure
//
// Arguments:
//    jitInterface - callback interface from jit to VM
//    localCount - number of original method locals to report live
//    ilSize - ilSize of original method method (for sanity check)
//    fpToSpDelta - offset of FP to SP for the original method
//
// Return Value:
//    Pointer to newly allocated PatchpointInfo structure.
//
// Notes:
//    The runtime will also associate this structure with the original
//    method, so that it can be retrieved and passed back to the jit
//    during an OSR rejit request.

PatchpointInfo* PatchpointInfo::Allocate(ICorJitInfo* jitInterface,
                                         unsigned     localCount,
                                         unsigned     ilSize,
                                         int          fpToSpDelta)
{
    int             baseSize     = sizeof(PatchpointInfo);
    int             variableSize = localCount * sizeof(int);
    PatchpointInfo* result       = (PatchpointInfo*)jitInterface->allocPatchpointInfo(baseSize + variableSize);

    result->m_ilSize         = ilSize;
    result->m_fpToSpDelta    = fpToSpDelta;
    result->m_numberOfLocals = localCount;

    return result;
}

//------------------------------------------------------------------------
// IsExposed: check if a local was address exposed in the original method
//
// Arguments:
//    localNum - number of the local in question (in IL numbering)
//
// Return Value:
//    True if the local was exposed; if so the OSR method must refer to
//    the local using the original stack location.

bool PatchpointInfo::IsExposed(unsigned localNum) const
{
    assert(localNum < m_numberOfLocals);

    return (m_offsetAndExposureData[localNum] & exposureMask);
}

//------------------------------------------------------------------------
// SetIsExposed: mark a local as address exposed in the original method
//
// Arguments:
//    localNum - number of the local in question (in IL numbering)

void PatchpointInfo::SetIsExposed(unsigned localNum)
{
    assert(localNum < m_numberOfLocals);
    assert((m_offsetAndExposureData[localNum] & exposureMask) == 0);

    m_offsetAndExposureData[localNum] |= exposureMask;
}

//------------------------------------------------------------------------
// Offset: get the FP relative offset of a local in the original method
//
// Arguments:
//    localNum - number of the local in question (in IL numbering)
//
// Return Value:
//    Offset from FP, in bytes

int PatchpointInfo::Offset(unsigned localNum) const
{
    assert(localNum < m_numberOfLocals);

    return (m_offsetAndExposureData[localNum] & ~exposureMask);
}

//------------------------------------------------------------------------
// SetOffset: set the FP relative offset for a local in the original method
//
// Arguments:
//    localNum - number of the local in question (in IL numbering)
//    offset - offset from FP, in bytes

void PatchpointInfo::SetOffset(unsigned localNum, int offset)
{
    assert(localNum < m_numberOfLocals);
    assert((m_offsetAndExposureData[localNum] & ~exposureMask) == 0);
    assert((offset & exposureMask) == 0);

    m_offsetAndExposureData[localNum] |= offset;
}
