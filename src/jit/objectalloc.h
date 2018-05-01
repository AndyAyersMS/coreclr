// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                         ObjectAllocator                                   XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

/*****************************************************************************/
#ifndef OBJECTALLOC_H
#define OBJECTALLOC_H
/*****************************************************************************/

//===============================================================================
#include "phase.h"

class ObjectAllocator final : public Phase
{
    //===============================================================================
    // Data members
    bool         m_IsObjectStackAllocationEnabled;
    bool         m_AnalysisDone;
    bool         m_IsRunningAfterMorph;
    BitVecTraits m_bitVecTraits;
    BitVec       m_EscapingPointers;

    //===============================================================================
    // Methods
public:
    ObjectAllocator(Compiler* comp, bool isAfterMorph = true);
    bool IsObjectStackAllocationEnabled() const;
    void EnableObjectStackAllocation();
    bool IsRunningAfterMorph() const;

protected:
    virtual void DoPhase() override;

private:
    bool CanAllocateLclVarOnStack(unsigned int lclNum, CORINFO_CLASS_HANDLE clsHnd);
    bool CanLclVarEscape(unsigned int lclNum);
    void DoAnalysis();
    void BuildConnGraph(BitVec** pConnGraphPointees);
    static void ComputeReachableNodes(BitVecTraits* bitVecTraits, BitVec* adjacentNodes, BitVec& reachableNodes);
    void     MorphAllocObjNodes();
    GenTree* MorphAllocObjNodeIntoHelperCall(GenTreeAllocObj* allocObj);
    GenTree* MorphAllocObjNodeIntoStackAlloc(GenTreeAllocObj* allocObj, BasicBlock* block, GenTreeStmt* stmt);
    static bool CanLclVarEscapeViaParentStack(ArrayStack<GenTree*>* parentStack,
                                              Compiler*             compiler,
                                              unsigned int          lclNum);
    static Compiler::fgWalkResult BuildConnGraphVisitor(GenTree** pTree, Compiler::fgWalkData* data);
    struct BuildConnGraphVisitorCallbackData;
#ifdef DEBUG
    static Compiler::fgWalkResult AssertWhenAllocObjFoundVisitor(GenTree** pTree, Compiler::fgWalkData* data);
#endif // DEBUG
    static const unsigned int s_StackAllocMaxSize = 0x2000U;
};

//===============================================================================

inline ObjectAllocator::ObjectAllocator(Compiler* comp, bool isAfterMorph)
    : Phase(comp, "Allocate Objects", PHASE_ALLOCATE_OBJECTS)
    , m_IsObjectStackAllocationEnabled(false)
    , m_AnalysisDone(false)
    , m_IsRunningAfterMorph(isAfterMorph)
    , m_bitVecTraits(comp->lvaCount, comp)
{
    m_EscapingPointers = BitVecOps::UninitVal();
    m_doChecks         = isAfterMorph;
}

inline bool ObjectAllocator::IsObjectStackAllocationEnabled() const
{
    return m_IsObjectStackAllocationEnabled;
}

inline void ObjectAllocator::EnableObjectStackAllocation()
{
    m_IsObjectStackAllocationEnabled = true;
}

inline bool ObjectAllocator::IsRunningAfterMorph() const
{
    return m_IsRunningAfterMorph;
}

//------------------------------------------------------------------------
// CanAllocateLclVarOnStack: Returns true iff local variable can be
//                           allocated on the stack.
inline bool ObjectAllocator::CanAllocateLclVarOnStack(unsigned int lclNum, CORINFO_CLASS_HANDLE clsHnd)
{
    assert(m_AnalysisDone);

    const BOOL         hasFinalizer = comp->info.compCompHnd->classHasFinalizer(clsHnd);
    const BOOL         isValueClass = comp->info.compCompHnd->isValueClass(clsHnd);
    const unsigned int classSize =
        isValueClass ? comp->info.compCompHnd->getClassSize(clsHnd) : comp->info.compCompHnd->getHeapClassSize(clsHnd);

    return !CanLclVarEscape(lclNum) && !hasFinalizer && classSize <= s_StackAllocMaxSize;
}

//------------------------------------------------------------------------
// CanLclVarEscape:          Returns true iff local variable can
//                           potentially escape from the method
inline bool ObjectAllocator::CanLclVarEscape(unsigned int lclNum)
{
    assert(m_AnalysisDone);
    return BitVecOps::IsMember(&m_bitVecTraits, m_EscapingPointers, lclNum);
}

//===============================================================================

#endif // OBJECTALLOC_H
