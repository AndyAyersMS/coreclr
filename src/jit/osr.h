// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#ifndef _OSR_H_
#define _OSR_H_

// On-Stack Replacement Data Structures

class ICorJitInfo;

// PatchpointInfo is created by the jit during Tier0 compilations for
// methods with patchpoints. It is associated with the method and is
// passed back by the runtime to the jit for OSR requests on the
// method.
//
// This version of the OSRInfo expects that all patchpoints will
// report the same live set.

class PatchpointInfo
{
public:
    static PatchpointInfo* Allocate(ICorJitInfo* jitInterface, unsigned localCount, unsigned ilSize, int fpToSpDelta);

    unsigned ILSize() const
    {
        return m_ilSize;
    }

    int FpToSpDelta() const
    {
        return m_fpToSpDelta;
    }

    unsigned NumberOfLocals() const
    {
        return m_numberOfLocals;
    }

    int GenericContextArgOffset() const
    {
        return m_genericContextArgOffset;
    }

    void SetGenericContextArgOffset(int offset)
    {
        m_genericContextArgOffset = offset;
    }

    bool IsExposed(unsigned localNum) const;
    void SetIsExposed(unsigned localNum);

    int Offset(unsigned localNum) const;
    void SetOffset(unsigned localNum, int offset);

private:
    enum
    {
        EXPOSURE_MASK = 0x1
    };

    unsigned m_ilSize;
    unsigned m_numberOfLocals;
    int      m_fpToSpDelta;
    int      m_genericContextArgOffset;
    int      m_offsetAndExposureData[];
};

#endif // _OSR_H_
