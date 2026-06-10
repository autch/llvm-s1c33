//===--- PIECE.cpp - Aquaplus P/ECE ToolChain -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PIECE.h"

#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Options/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace llvm::opt;
using namespace clang;
using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang::driver::toolchains;

Tool *PIECEToolChain::buildLinker() const {
  return new tools::piece::Linker(*this);
}

void piece::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output,
                                 const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  const auto &TC =
      static_cast<const toolchains::PIECEToolChain &>(getToolChain());
  const Driver &D = TC.getDriver();

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));

  CmdArgs.push_back("-Bstatic");

  if (const char *LDMOption = getLDMOption(TC.getTriple(), Args)) {
    CmdArgs.push_back("-m");
    CmdArgs.push_back(LDMOption);
  }

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles,
                   options::OPT_r)) {
    // crt0.o clears BSS, sets up the heap, and dispatches the kernel's
    // pceAppInit/pceAppProc/pceAppExit callbacks; crti.o provides the
    // default pceNotify handler.
    CmdArgs.push_back(Args.MakeArgString(TC.GetFilePath("crt0.o")));
    std::string CrtiPath = TC.GetFilePath("crti.o");
    if (D.getVFS().exists(CrtiPath))
      CmdArgs.push_back(Args.MakeArgString(CrtiPath));
  }

  // Inject the default P/ECE linker script if the user did not pass -T.
  if (!Args.hasArg(options::OPT_T_Group)) {
    SmallString<128> DefaultLD(TC.computeSysRoot());
    llvm::sys::path::append(DefaultLD, "lib", "piece.ld");
    if (D.getVFS().exists(DefaultLD))
      CmdArgs.push_back(Args.MakeArgString("-T" + DefaultLD));
  }

  Args.addAllArgs(CmdArgs,
                  {options::OPT_L, options::OPT_u, options::OPT_T_Group,
                   options::OPT_s, options::OPT_t, options::OPT_r});

  TC.AddFilePathLibArgs(Args, CmdArgs);

  for (const auto &LibPath : TC.getLibraryPaths())
    CmdArgs.push_back(Args.MakeArgString(llvm::Twine("-L", LibPath)));

  if (D.isUsingLTO())
    addLTOOptions(TC, Args, CmdArgs, Output, Inputs,
                  D.getLTOMode() == LTOK_Thin);

  AddLinkerInputs(TC, Inputs, Args, CmdArgs, JA);

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs)) {
    // P/ECE default libraries in the canonical link order:
    // libclang_rt.builtins-s1c33.a provides compiler-rt builtins (FP,
    // integer division, 64-bit arithmetic); libcxxrt is the C++ runtime;
    // libpceapi holds the P/ECE kernel API stubs; libc/libm come from
    // newlib.  --start-group/--end-group handles circular references
    // between the libraries.
    //
    // libpceshim sits ahead of -lc to shadow newlib's rand/srand and
    // __assert_func with tiny single-threaded versions, breaking those
    // symbols' dependency cascade into malloc and stdio.
    CmdArgs.push_back("-lclang_rt.builtins-s1c33");
    CmdArgs.push_back("--start-group");
    CmdArgs.push_back("-lcxxrt");
    CmdArgs.push_back("-lpceapi");
    CmdArgs.push_back("-lpceshim");
    CmdArgs.push_back("-lc");
    CmdArgs.push_back("-lm");
    CmdArgs.push_back("--end-group");
  }

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::AtFileCurCP(),
      Args.MakeArgString(TC.GetLinkerPath()), CmdArgs, Inputs, Output));
}
