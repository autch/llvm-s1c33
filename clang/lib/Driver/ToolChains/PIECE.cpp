//===--- PIECE.cpp - Aquaplus P/ECE ToolChain -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PIECE.h"

#include "clang/Basic/DiagnosticDriver.h"
#include "clang/Driver/CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Options/Options.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace llvm::opt;
using namespace clang;
using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang::driver::toolchains;

// Map a -mprintf=/-mscanf= variant spelling to the picolibc backend letter
// used in the __<letter>_vfprintf / __<letter>_vfscanf symbol names.
// Returns 0 for an unrecognised spelling.
static char piecePrintfVariantLetter(StringRef V) {
  return llvm::StringSwitch<char>(V)
      .Cases("double", "d", 'd')
      .Cases("float", "f", 'f')
      .Cases("long-long", "l", 'l')
      .Cases("integer", "i", 'i')
      .Cases("minimal", "m", 'm')
      .Default(0);
}

// picolibc selects the printf/scanf variant at link time by aliasing the
// generic vfprintf/vfscanf symbol to one of the per-variant backends
// (__d_/__f_/__l_/__i_/__m_).  picolibc's GCC specs do this from
// --printf=/--scanf=; we are not using those specs, so translate the
// S1C33-specific -mprintf=/-mscanf= driver options into the same
// --defsym aliases here.  With no option the library default (built with
// -Dformat-default=double) is used.  NOTE: --defsym aliasing does not work
// under LTO; an LTO build always gets the library default.
static void addPiecePrintfDefsym(const ArgList &Args, ArgStringList &CmdArgs,
                                 const Driver &D) {
  struct {
    options::ID OptID;
    const char *Sym;
  } Kinds[] = {
      {options::OPT_mprintf_EQ, "vfprintf"},
      {options::OPT_mscanf_EQ, "vfscanf"},
  };
  for (const auto &K : Kinds) {
    if (Arg *A = Args.getLastArg(K.OptID)) {
      StringRef V = A->getValue();
      char L = piecePrintfVariantLetter(V);
      if (!L) {
        D.Diag(diag::err_drv_invalid_value) << A->getAsString(Args) << V;
        continue;
      }
      // e.g. --defsym=vfprintf=__i_vfprintf .  Built as std::string because
      // Twine cannot concatenate a bare char.
      std::string DefSym =
          std::string("--defsym=") + K.Sym + "=__" + L + "_" + K.Sym;
      CmdArgs.push_back(Args.MakeArgString(DefSym));
    }
  }
}

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
    // Optional picolibc printf/scanf variant selection (-mprintf=/-mscanf=)
    // is emitted as linker --defsym aliases ahead of the libraries.
    addPiecePrintfDefsym(Args, CmdArgs, D);

    // P/ECE default libraries in the canonical link order:
    // libclang_rt.builtins-s1c33.a provides compiler-rt builtins (FP,
    // integer division, 64-bit arithmetic); libcxxrt is the C++ runtime;
    // libpceapi holds the P/ECE kernel API stubs; libpicortt is the
    // picolibc retarget layer (stdout-discard console + sbrk); libc/libm
    // come from picolibc.  --start-group/--end-group handles circular
    // references between the libraries.
    //
    // libpicortt sits ahead of -lc so its strong stdin/stdout/stderr and
    // sbrk override picolibc's weak defaults (and keep picolibc's
    // picosbrk.o, which references __heap_start/__heap_end, from being
    // pulled in).  libpceshim sits ahead of -lc to shadow picolibc's
    // __assert_func (which would otherwise drag in stdio).
    CmdArgs.push_back("-lclang_rt.builtins-s1c33");
    CmdArgs.push_back("--start-group");
    CmdArgs.push_back("-lcxxrt");
    CmdArgs.push_back("-lpceapi");
    CmdArgs.push_back("-lpicortt");
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
