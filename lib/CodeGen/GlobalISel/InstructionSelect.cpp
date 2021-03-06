//===- llvm/CodeGen/GlobalISel/InstructionSelect.cpp - InstructionSelect ---==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements the InstructionSelect class.
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetSubtargetInfo.h"

#define DEBUG_TYPE "instruction-select"

using namespace llvm;

char InstructionSelect::ID = 0;
INITIALIZE_PASS(InstructionSelect, DEBUG_TYPE,
                "Select target instructions out of generic instructions",
                false, false);

InstructionSelect::InstructionSelect() : MachineFunctionPass(ID) {
  initializeInstructionSelectPass(*PassRegistry::getPassRegistry());
}

static void reportSelectionError(const MachineInstr &MI, const Twine &Message) {
  const MachineFunction &MF = *MI.getParent()->getParent();
  std::string ErrStorage;
  raw_string_ostream Err(ErrStorage);
  Err << Message << ":\nIn function: " << MF.getName() << '\n' << MI << '\n';
  report_fatal_error(Err.str());
}

bool InstructionSelect::runOnMachineFunction(MachineFunction &MF) {
  DEBUG(dbgs() << "Selecting function: " << MF.getName() << '\n');

  const InstructionSelector *ISel = MF.getSubtarget().getInstructionSelector();
  assert(ISel && "Cannot work without InstructionSelector");

  // FIXME: freezeReservedRegs is now done in IRTranslator, but there are many
  // other MF/MFI fields we need to initialize.

#ifndef NDEBUG
  // FIXME: We could introduce new blocks and will need to fix the outer loop.
  // Until then, keep track of the number of blocks to assert that we don't.
  const size_t NumBlocks = MF.size();
#endif

  for (MachineBasicBlock *MBB : post_order(&MF)) {
    for (MachineBasicBlock::reverse_iterator MII = MBB->rbegin(),
                                             End = MBB->rend();
         MII != End;) {
      MachineInstr &MI = *MII++;
      DEBUG(dbgs() << "Selecting: " << MI << '\n');
      if (!ISel->select(MI))
        reportSelectionError(MI, "Cannot select");
      // FIXME: It would be nice to dump all inserted instructions.  It's not
      // obvious how, esp. considering select() can insert after MI.
    }
  }

  assert(MF.size() == NumBlocks && "Inserting blocks is not supported yet");

  // Check that we did select everything. Do this separately to make sure we
  // didn't miss any newly inserted instructions.
  // FIXME: This (and other checks) should move into a verifier, predicated on
  // a "post-isel" MachineFunction property. That would also let us selectively
  // enable it depending on build configuration.
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (isPreISelGenericOpcode(MI.getOpcode())) {
        reportSelectionError(
            MI, "Generic instruction survived instruction selection");
      }
    }
  }

  // Now that selection is complete, there are no more generic vregs.
  // FIXME: We're still discussing what to do with the vreg->size map:
  // it's somewhat redundant (with the def MIs type size), but having to
  // examine MIs is also awkward.  Another alternative is to track the type on
  // the vreg instead, but that's not ideal either, because it's saying that
  // vregs have types, which they really don't. But then again, LLT is just
  // a size and a "shape": it's probably the same information as regbank info.
  MF.getRegInfo().clearVirtRegSizes();

  // FIXME: Should we accurately track changes?
  return true;
}
