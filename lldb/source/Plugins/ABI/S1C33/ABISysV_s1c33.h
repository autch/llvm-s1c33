//===-- ABISysV_s1c33.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// LLDB ABI plugin for the EPSON S1C33000 processor family (S5U1C33000C ABI).
//
// ABI summary:
//   Return value:   R10 (R10+R11 for 64-bit)
//   Arguments:      R12, R13, R14, R15 (overflow to stack)
//   Callee-saved:   R0, R1, R2, R3
//   Scratch:        R4, R5, R6, R7, R9
//   Reserved:       R8 (kernel table base)
//   Stack:          SP-based, no frame pointer, grows down
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_ABISysV_s1c33_h_
#define liblldb_ABISysV_s1c33_h_

#include "llvm/TargetParser/Triple.h"

#include "lldb/Target/ABI.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Flags.h"
#include "lldb/lldb-private.h"

class ABISysV_s1c33 : public lldb_private::RegInfoBasedABI {
public:
  ~ABISysV_s1c33() override = default;

  size_t GetRedZoneSize() const override { return 0; }

  bool PrepareTrivialCall(lldb_private::Thread &thread, lldb::addr_t sp,
                          lldb::addr_t functionAddress,
                          lldb::addr_t returnAddress,
                          llvm::ArrayRef<lldb::addr_t> args) const override;

  bool GetArgumentValues(lldb_private::Thread &thread,
                         lldb_private::ValueList &values) const override;

  lldb_private::Status
  SetReturnValueObject(lldb::StackFrameSP &frame_sp,
                       lldb::ValueObjectSP &new_value) override;

  lldb::ValueObjectSP
  GetReturnValueObjectImpl(lldb_private::Thread &thread,
                           lldb_private::CompilerType &type) const override;

  lldb::UnwindPlanSP CreateFunctionEntryUnwindPlan() override;

  lldb::UnwindPlanSP CreateDefaultUnwindPlan() override;

  bool RegisterIsVolatile(const lldb_private::RegisterInfo *reg_info) override;

  // The S1C33 address space is 28-bit; any word-aligned address is valid as CFA.
  bool CallFrameAddressIsValid(lldb::addr_t cfa) override {
    return cfa != 0 && (cfa & 0x3) == 0;
  }

  bool CodeAddressIsValid(lldb::addr_t pc) override {
    // S1C33 instructions are 16-bit aligned.
    return (pc & 0x1) == 0;
  }

  const lldb_private::RegisterInfo *
  GetRegisterInfoArray(uint32_t &count) override;

  static lldb::ABISP CreateInstance(lldb::ProcessSP process_sp,
                                    const lldb_private::ArchSpec &arch);

  static void Initialize();
  static void Terminate();

  static llvm::StringRef GetPluginNameStatic() { return "s1c33"; }

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

  void AugmentRegisterInfo(
      std::vector<lldb_private::DynamicRegisterInfo::Register> &regs) override;

private:
  bool RegisterIsCalleeSaved(const lldb_private::RegisterInfo *reg_info);

  using lldb_private::RegInfoBasedABI::RegInfoBasedABI;
};

#endif // liblldb_ABISysV_s1c33_h_
