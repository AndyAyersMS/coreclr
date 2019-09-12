// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

#include "osr.h"

OSRInfo* OSRInfo::Allocate(ICorJitInfo* jitInterface, int localCount, int ilSize, int fpToSpDelta)
{
    int baseSize = sizeof(OSRInfo);
    int variableSize = localCount * sizeof(int);
    OSRInfo* result = jitInterface->allocatePatchpointInfo(baseSize + variableSize);

    result->m_ilSize = ilSize;
    result->m_fpToSpDelta = fpToSpDelta;
    result->m_numberOfLocals = localCount;

    return result;
}

bool OSRInfo::IsExposed(int localNum) const
{
    assert(localNum >= 0 && localNum < m_numberOfLocals);
    
    return (m_offsetAndExposureData[localNum] & exposureMask);
}

void OSRInfo::SetIsExposed(int localNum)
{
    assert(localNum >= 0 && localNum < m_numberOfLocals);
    assert((m_offsetAndExposureData[localNum] & exposureMask) == 0);
    
    m_offsetAndExposureData[localNum] |= exposureMask;
}

int OSRInfo::Offset(int localNum) const
{
    assert(localNum >= 0 && localNum < m_numberOfLocals);
    
    return (m_offsetAndExposureData[localNum] & ~exposureMask);
}

void OSRInfo::SetOffset(int localNum, int offset)
{
    assert(localNum >= 0 && localNum < m_numberOfLocals);
    assert((m_offsetAndExposureData[localNum] & ~exposureMask) == 0);
    assert(offset & exposureMask == 0);
    
    m_offsetAndExposureData[localNum] |= offset;
}



