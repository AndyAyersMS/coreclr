// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

// Likely will/would become a class hierarchy based on strategy
// For now, uses one counter per frame.
class PatchpointTransformer
{
    unsigned  ppCounterLclNum;
    const int HIGH_PROBABILITY = 99;
    Compiler* compiler;

public:
    PatchpointTransformer(Compiler* compiler) : compiler(compiler)
    {
        ppCounterLclNum = compiler->lvaGrabTemp(true DEBUGARG("patchpoint counter"));
    }

    //------------------------------------------------------------------------
    // Run: run transformation for each block.
    //
    // Returns:
    //   Number of patchpoints transformed.
    int Run()
    {
        int count = 0;

        for (BasicBlock* block = compiler->fgFirstBB; block != nullptr; block = block->bbNext)
        {
            if (block->bbFlags & BBF_PATCHPOINT)
            {
                TransformBlock(block);
                count++;
            }
        }

        return count;
    }

private:
    //------------------------------------------------------------------------
    // CreateAndInsertBasicBlock: ask compiler to create new basic block.
    // and insert in into the basic block list.
    //
    // Arguments:
    //    jumpKind - jump kind for the new basic block
    //    insertAfter - basic block, after which compiler has to insert the new one.
    //
    // Return Value:
    //    new basic block.
    BasicBlock* CreateAndInsertBasicBlock(BBjumpKinds jumpKind, BasicBlock* insertAfter)
    {
        BasicBlock* block = compiler->fgNewBBafter(jumpKind, insertAfter, true);
        if ((insertAfter->bbFlags & BBF_INTERNAL) == 0)
        {
            block->bbFlags &= ~BBF_INTERNAL;
            block->bbFlags |= BBF_IMPORTED;
        }
        return block;
    }

    //------------------------------------------------------------------------
    // TransformBlock: expand current block to include patchpoint logic.
    //
    //  S;
    //
    //  ==>
    //
    //  if (--ppCounter < 0)
    //  {
    //     ppHelper(&ppCounter);
    //  }
    //  S;
    //
    void TransformBlock(BasicBlock* block)
    {
        // Current block now becomes the test block
        BasicBlock* remainderBlock = compiler->fgSplitBlockAtBeginning(block);
        BasicBlock* helperBlock    = CreateAndInsertBasicBlock(BBJ_ALWAYS, block);

        // Update flow
        block->bbJumpKind = BBJ_COND;
        block->bbJumpDest = remainderBlock;

        // Update weights
        remainderBlock->inheritWeight(block);
        helperBlock->inheritWeightPercentage(block, 100 - HIGH_PROBABILITY);

        // Fill in test block
        // --ppCounter;
        GenTree*     ppCounterBefore = compiler->gtNewLclvNode(ppCounterLclNum, TYP_INT);
        GenTree*     ppCounterAfter  = compiler->gtNewLclvNode(ppCounterLclNum, TYP_INT);
        GenTree*     one             = compiler->gtNewIconNode(1, TYP_INT);
        GenTree*     ppCounterSub    = compiler->gtNewOperNode(GT_SUB, TYP_INT, ppCounterBefore, one);
        GenTree*     ppCounterAsg    = compiler->gtNewOperNode(GT_ASG, TYP_INT, ppCounterAfter, ppCounterSub);
        GenTreeStmt* asgStmt         = compiler->fgNewStmtFromTree(ppCounterAsg);
        compiler->fgInsertStmtAtEnd(block, asgStmt);

        // if (ppCounter < 0)
        GenTree*     ppCounterUpdated = compiler->gtNewLclvNode(ppCounterLclNum, TYP_INT);
        GenTree*     zero             = compiler->gtNewIconNode(0, TYP_INT);
        GenTree*     compare          = compiler->gtNewOperNode(GT_LT, TYP_INT, ppCounterUpdated, zero);
        GenTree*     jmp              = compiler->gtNewOperNode(GT_JTRUE, TYP_VOID, compare);
        GenTreeStmt* jmpStmt          = compiler->fgNewStmtFromTree(jmp);
        compiler->fgInsertStmtAtEnd(block, jmpStmt);

        // Fill in helper block
        // call PPHelper(&ppCounter)
        GenTree*        ppCounterRef  = compiler->gtNewLclvNode(ppCounterLclNum, TYP_INT);
        GenTree*        ppCounterAddr = compiler->gtNewOperNode(GT_ADDR, TYP_I_IMPL, ppCounterRef);
        GenTreeArgList* helperArgs    = compiler->gtNewArgList(ppCounterAddr);
        GenTreeCall*    helperCall    = compiler->gtNewHelperCallNode(CORINFO_HELP_PATCHPOINT, TYP_VOID, helperArgs);
        compiler->fgInsertStmtAtEnd(helperBlock, helperCall);
    }
};

// Expansion of patchpoints into control flow.
//
// Patchpoints are placed in the JIT IR during importation, and get expanded
// here into normal JIT IR.

void Compiler::fgTransformPatchpoints()
{
    JITDUMP("\n*************** in fgTransformPatchpoints\n");

    assert(!compIsForInlining());

    if (!doesMethodHavePatchpoints())
    {
        JITDUMP(" -- no patchpoints to transform\n");
        return;
    }

    PatchpointTransformer ppTransformer(this);
    int                   count = ppTransformer.Run();

    assert(count > 0);

    JITDUMP("\n*************** After fgTransformPatchpoints() [%d patchpoints transformed]\n", count);
    INDEBUG(if (verbose) { fgDispBasicBlocks(true); });
}
