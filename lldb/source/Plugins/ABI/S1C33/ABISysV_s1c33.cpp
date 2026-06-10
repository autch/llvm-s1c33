//===-- ABISysV_s1c33.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// LLDB ABI plugin for the EPSON S1C33000 processor family (S5U1C33000C ABI).
//
// Register layout (DWARF numbers match S1C33RegisterInfo.td):
//   R0-R15  = DWARF 0-15
//   SP      = DWARF 16
//   PC      = DWARF 17
//   PSR     = DWARF 18
//
// This plugin primarily supports GDB-remote connections to an RSP emulator
// that provides register info via qXfer:features:read.  The register stubs
// here are used as a fallback when the XML is unavailable.
//
//===----------------------------------------------------------------------===//

#include "ABISysV_s1c33.h"

#include <array>

#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/DerivedTypes.h"

#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Value.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"

#define DEFINE_REG_NAME(reg_num) ConstString(#reg_num).GetCString()
#define DEFINE_REG_NAME_STR(reg_name) ConstString(reg_name).GetCString()

// The ABI is not the primary source of register size/offset/encoding; it just
// provides the correct DWARF numbers so LLDB can correlate debug info.
#define DEFINE_GENERIC_REGISTER_STUB(dwarf_num, str_name, generic_num)         \
  {                                                                            \
      DEFINE_REG_NAME(dwarf_num),                                              \
      DEFINE_REG_NAME_STR(str_name),                                           \
      0,                                                                       \
      0,                                                                       \
      eEncodingInvalid,                                                        \
      eFormatDefault,                                                          \
      {dwarf_num, dwarf_num, generic_num, LLDB_INVALID_REGNUM, dwarf_num},     \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

#define DEFINE_REGISTER_STUB(dwarf_num, str_name)                              \
  DEFINE_GENERIC_REGISTER_STUB(dwarf_num, str_name, LLDB_INVALID_REGNUM)

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE_ADV(ABISysV_s1c33, ABIS1C33)

namespace {
namespace dwarf {
// DWARF register numbers (must match S1C33RegisterInfo.td DwarfRegNum values).
enum regnums {
  r0 = 0,
  r1 = 1,
  r2 = 2,
  r3 = 3,
  r4 = 4,
  r5 = 5,
  r6 = 6,
  r7 = 7,
  r8 = 8, // kernel table base (reserved)
  r9 = 9,
  r10 = 10, // return value
  r11 = 11, // return value high (64-bit)
  r12 = 12, // arg0
  r13 = 13, // arg1
  r14 = 14, // arg2
  r15 = 15, // arg3
  sp = 16,
  pc = 17,
  psr = 18,
};

static const std::array<RegisterInfo, 19> g_register_infos = {{
    DEFINE_REGISTER_STUB(r0, "r0"),
    DEFINE_REGISTER_STUB(r1, "r1"),
    DEFINE_REGISTER_STUB(r2, "r2"),
    DEFINE_REGISTER_STUB(r3, "r3"),
    DEFINE_REGISTER_STUB(r4, "r4"),
    DEFINE_REGISTER_STUB(r5, "r5"),
    DEFINE_REGISTER_STUB(r6, "r6"),
    DEFINE_REGISTER_STUB(r7, "r7"),
    DEFINE_REGISTER_STUB(r8, "r8"),
    DEFINE_REGISTER_STUB(r9, "r9"),
    DEFINE_GENERIC_REGISTER_STUB(r10, "r10", LLDB_REGNUM_GENERIC_ARG1),
    DEFINE_REGISTER_STUB(r11, "r11"),
    DEFINE_GENERIC_REGISTER_STUB(r12, "r12", LLDB_REGNUM_GENERIC_ARG2),
    DEFINE_GENERIC_REGISTER_STUB(r13, "r13", LLDB_REGNUM_GENERIC_ARG3),
    DEFINE_GENERIC_REGISTER_STUB(r14, "r14", LLDB_REGNUM_GENERIC_ARG4),
    DEFINE_REGISTER_STUB(r15, "r15"),
    DEFINE_GENERIC_REGISTER_STUB(sp, "sp", LLDB_REGNUM_GENERIC_SP),
    DEFINE_GENERIC_REGISTER_STUB(pc, "pc", LLDB_REGNUM_GENERIC_PC),
    DEFINE_REGISTER_STUB(psr, "psr"),
}};
} // namespace dwarf
} // namespace

const RegisterInfo *ABISysV_s1c33::GetRegisterInfoArray(uint32_t &count) {
  count = dwarf::g_register_infos.size();
  return dwarf::g_register_infos.data();
}

ABISP ABISysV_s1c33::CreateInstance(ProcessSP process_sp,
                                    const ArchSpec &arch) {
  if (arch.GetTriple().getArch() != llvm::Triple::s1c33)
    return ABISP();
  return ABISP(
      new ABISysV_s1c33(std::move(process_sp), MakeMCRegisterInfo(arch)));
}

bool ABISysV_s1c33::PrepareTrivialCall(Thread &thread, addr_t sp,
                                       addr_t functionAddress,
                                       addr_t returnAddress,
                                       llvm::ArrayRef<addr_t> args) const {
  // Not currently implemented; GDB-remote debugging does not need this.
  return false;
}

bool ABISysV_s1c33::GetArgumentValues(Thread &thread, ValueList &values) const {
  return false;
}

Status ABISysV_s1c33::SetReturnValueObject(StackFrameSP &frame_sp,
                                           ValueObjectSP &new_value) {
  return Status::FromErrorString(
      "ABISysV_s1c33::SetReturnValueObject not implemented");
}

ValueObjectSP
ABISysV_s1c33::GetReturnValueObjectImpl(Thread &thread,
                                        CompilerType &type) const {
  return ValueObjectSP();
}

lldb::UnwindPlanSP ABISysV_s1c33::CreateFunctionEntryUnwindPlan() {
  // At function entry on S1C33, the call instruction has:
  //   - decremented SP by 4
  //   - stored the return address at [SP] (the new top of stack)
  // So CFA = SP + 4, and the return PC is at [SP+0] (= CFA - 4).

  UnwindPlan::Row row;
  row.GetCFAValue().SetIsRegisterPlusOffset(dwarf::sp, 4);

  // Return address is at [SP+0], i.e. [CFA-4].
  row.SetRegisterLocationToAtCFAPlusOffset(dwarf::pc, -4, true);

  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindDWARF);
  plan_sp->AppendRow(std::move(row));
  plan_sp->SetSourceName("s1c33 function-entry unwind plan");
  plan_sp->SetSourcedFromCompiler(eLazyBoolNo);
  plan_sp->SetUnwindPlanForSignalTrap(eLazyBoolNo);
  return plan_sp;
}

lldb::UnwindPlanSP ABISysV_s1c33::CreateDefaultUnwindPlan() {
  // Conservative fallback: CFA = SP, return PC at [SP+0].
  // This matches the state just after a call instruction.
  UnwindPlan::Row row;
  row.GetCFAValue().SetIsRegisterPlusOffset(dwarf::sp, 0);
  row.SetRegisterLocationToAtCFAPlusOffset(dwarf::pc, 0, true);

  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindDWARF);
  plan_sp->AppendRow(std::move(row));
  plan_sp->SetSourceName("s1c33 default unwind plan");
  plan_sp->SetSourcedFromCompiler(eLazyBoolNo);
  plan_sp->SetUnwindPlanValidAtAllInstructions(eLazyBoolNo);
  plan_sp->SetUnwindPlanForSignalTrap(eLazyBoolNo);
  return plan_sp;
}

bool ABISysV_s1c33::RegisterIsVolatile(const RegisterInfo *reg_info) {
  return !RegisterIsCalleeSaved(reg_info);
}

bool ABISysV_s1c33::RegisterIsCalleeSaved(const RegisterInfo *reg_info) {
  if (!reg_info)
    return false;

  // Callee-saved: R0, R1, R2, R3, SP, PC (frame).
  // Reserved R8 is treated as callee-saved (never modified by user code).
  return llvm::StringSwitch<bool>(reg_info->name)
      .Cases("r0", "r1", "r2", "r3", true)
      .Cases("r8", "sp", "pc", true)
      .Default(false);
}

void ABISysV_s1c33::AugmentRegisterInfo(
    std::vector<DynamicRegisterInfo::Register> &regs) {
  lldb_private::RegInfoBasedABI::AugmentRegisterInfo(regs);

  for (auto &reg : regs) {
    if (reg.name == "pc")
      reg.regnum_generic = LLDB_REGNUM_GENERIC_PC;
    else if (reg.name == "sp")
      reg.regnum_generic = LLDB_REGNUM_GENERIC_SP;
    else if (reg.name == "r10")
      reg.regnum_generic = LLDB_REGNUM_GENERIC_ARG1;
    else if (reg.name == "r12")
      reg.regnum_generic = LLDB_REGNUM_GENERIC_ARG2;
    else if (reg.name == "r13")
      reg.regnum_generic = LLDB_REGNUM_GENERIC_ARG3;
    else if (reg.name == "r14")
      reg.regnum_generic = LLDB_REGNUM_GENERIC_ARG4;
  }
}

void ABISysV_s1c33::Initialize() {
  PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                "S5U1C33000C ABI for S1C33 targets",
                                CreateInstance);
}

void ABISysV_s1c33::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}
