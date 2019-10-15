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
        ppCounterLclNum                            = compiler->lvaGrabTemp(true DEBUGARG("patchpoint counter"));
        compiler->lvaTable[ppCounterLclNum].lvType = TYP_INT;
    }

    //------------------------------------------------------------------------
    // Run: run transformation for each block.
    //
    // Returns:
    //   Number of patchpoints transformed.
    int Run()
    {
        // If the first block is a patchpoint, insert a scratch block.
        if (compiler->fgFirstBB->bbFlags & BBF_PATCHPOINT)
        {
            compiler->fgEnsureFirstBBisScratch();
        }

        BasicBlock* block = compiler->fgFirstBB;
        TransformEntry(block);

        int count = 0;
        for (block = block->bbNext; block != nullptr; block = block->bbNext)
        {
            // If block is in a handler region, don't insert a patchpoint.
            // We can't OSR from funclets.
            if (compiler->ehGetBlockHndDsc(block) != nullptr)
            {
                JITDUMP("Patchpoint: skipping " FMT_BB " as it is in a handler\n", block->bbNum);
                continue;
            }

            // If block is in a try region, don't insert a patchpoint.
            // This is a temporary workaround until the OSR jit can
            // trim try regions to just the parts reachable via the patchpoint entry.
            if (compiler->ehGetBlockTryDsc(block) != nullptr)
            {
                JITDUMP("Patchpoint: [temporarily] skipping " FMT_BB " as it is in a try\n", block->bbNum);
                continue;
            }

            if (block->bbFlags & BBF_PATCHPOINT)
            {
                JITDUMP("Patchpoint: instrumenting " FMT_BB "\n", block->bbNum);
                assert(block != compiler->fgFirstBB);
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
    //     ppHelper(&ppCounter, ilOffset);
    //  }
    //  S;
    //
    void TransformBlock(BasicBlock* block)
    {
        // Capture the IL offset
        IL_OFFSET ilOffset = block->bbCodeOffs;
        assert(ilOffset != BAD_IL_OFFSET);

        // Current block now becomes the test block
        BasicBlock* remainderBlock = compiler->fgSplitBlockAtBeginning(block);
        BasicBlock* helperBlock    = CreateAndInsertBasicBlock(BBJ_NONE, block);

        // Update flow and flags
        block->bbJumpKind = BBJ_COND;
        block->bbJumpDest = remainderBlock;
        helperBlock->bbFlags |= BBF_BACKWARD_JUMP;

        // Update weights
        remainderBlock->inheritWeight(block);
        helperBlock->inheritWeightPercentage(block, 100 - HIGH_PROBABILITY);

        // Fill in test block
        //
        // --ppCounter;
        GenTree* ppCounterBefore = compiler->gtNewLclvNode(ppCounterLclNum, TYP_INT);
        GenTree* ppCounterAfter  = compiler->gtNewLclvNode(ppCounterLclNum, TYP_INT);
        GenTree* one             = compiler->gtNewIconNode(1, TYP_INT);
        GenTree* ppCounterSub    = compiler->gtNewOperNode(GT_SUB, TYP_INT, ppCounterBefore, one);
        GenTree* ppCounterAsg    = compiler->gtNewOperNode(GT_ASG, TYP_INT, ppCounterAfter, ppCounterSub);

        compiler->fgNewStmtAtEnd(block, ppCounterAsg);

        // if (ppCounter < 0)
        GenTree* ppCounterUpdated = compiler->gtNewLclvNode(ppCounterLclNum, TYP_INT);
        GenTree* zero             = compiler->gtNewIconNode(0, TYP_INT);
        GenTree* compare          = compiler->gtNewOperNode(GT_GE, TYP_INT, ppCounterUpdated, zero);
        GenTree* jmp              = compiler->gtNewOperNode(GT_JTRUE, TYP_VOID, compare);

        compiler->fgNewStmtAtEnd(block, jmp);

        // Fill in helper block
        //
        // call PPHelper(&ppCounter, ilOffset)
        GenTree*          ilOffsetNode  = compiler->gtNewIconNode(ilOffset, TYP_INT);
        GenTree*          ppCounterRef  = compiler->gtNewLclvNode(ppCounterLclNum, TYP_INT);
        GenTree*          ppCounterAddr = compiler->gtNewOperNode(GT_ADDR, TYP_I_IMPL, ppCounterRef);
        GenTreeCall::Use* helperArgs    = compiler->gtNewCallArgs(ppCounterAddr, ilOffsetNode);
        GenTreeCall*      helperCall    = compiler->gtNewHelperCallNode(CORINFO_HELP_PATCHPOINT, TYP_VOID, helperArgs);

        compiler->fgNewStmtAtEnd(helperBlock, helperCall);
    }

    //  ppCounter = 0 (could set it nonzero to save some cycles)
    void TransformEntry(BasicBlock* block)
    {
        assert((block->bbFlags & BBF_PATCHPOINT) == 0);

        GenTree* zero         = compiler->gtNewIconNode(0, TYP_INT);
        GenTree* ppCounterRef = compiler->gtNewLclvNode(ppCounterLclNum, TYP_INT);
        GenTree* ppCounterAsg = compiler->gtNewOperNode(GT_ASG, TYP_INT, ppCounterRef, zero);

        compiler->fgNewStmtNearEnd(block, ppCounterAsg);
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

    // We currently can't handle OSR in methods with localloc.
    // Such methods don't have a fixed relationship between frame and stack pointers.

    if (compLocallocUsed)
    {
        JITDUMP(" -- unable to handle methods with localloc\n");
        return;
    }

    // printf("@@@@ Placing patchpoints in %s\n", info.compFullName);

    PatchpointTransformer ppTransformer(this);
    int                   count = ppTransformer.Run();

    // assert(count > 0);

    JITDUMP("\n*************** After fgTransformPatchpoints() [%d patchpoints transformed]\n", count);
    INDEBUG(if (verbose) { fgDispBasicBlocks(true); });
}
