//===-- S1C33AsmParser.cpp - S1C33 assembly parser ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// S1C33 assembly syntax overview:
//
//   Register operands:  %r0 ... %r15, %sp, %alr, %ahr, %psr, %pc
//   Memory indirect:    [%rb]          -> register-indirect
//   SP-relative:        [%sp+imm6]     -> SP-relative displacement
//   Immediate:          integer literal or symbol reference
//
// The assembler uses ';' as the comment character (pp33 style).
//
// Extended mnemonics (x-prefix) are NOT supported here.
// Files using x-mnemonics must be preprocessed by asm33conv first.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/S1C33MCTargetDesc.h"
#include "S1C33.h"
#include "TargetInfo/S1C33TargetInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/MathExtras.h"

#define DEBUG_TYPE "s1c33-asm-parser"

using namespace llvm;

namespace {

//===----------------------------------------------------------------------===//
// S1C33Operand
//===----------------------------------------------------------------------===//

struct S1C33Operand : public MCParsedAsmOperand {
  enum KindTy { k_Token, k_Register, k_Immediate } Kind;

  SMLoc Start, End;

  struct RegOp {
    MCRegister Reg;
  };
  struct ImmOp {
    const MCExpr *Val;
  };
  struct TokOp {
    const char *Data;
    unsigned Length;
  };

  union {
    RegOp Reg;
    ImmOp Imm;
    TokOp Tok;
  };

  S1C33Operand(KindTy K, SMLoc S, SMLoc E) : Kind(K), Start(S), End(E) {}

  // Factory methods ---------------------------------------------------------

  static std::unique_ptr<S1C33Operand> CreateToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<S1C33Operand>(k_Token, S, S);
    Op->Tok.Data = Str.data();
    Op->Tok.Length = Str.size();
    return Op;
  }

  static std::unique_ptr<S1C33Operand> CreateReg(MCRegister Reg, SMLoc S,
                                                 SMLoc E) {
    auto Op = std::make_unique<S1C33Operand>(k_Register, S, E);
    Op->Reg.Reg = Reg;
    return Op;
  }

  static std::unique_ptr<S1C33Operand> CreateImm(const MCExpr *Val, SMLoc S,
                                                 SMLoc E) {
    auto Op = std::make_unique<S1C33Operand>(k_Immediate, S, E);
    Op->Imm.Val = Val;
    return Op;
  }

  // MCParsedAsmOperand overrides --------------------------------------------

  bool isToken() const override { return Kind == k_Token; }
  bool isReg() const override { return Kind == k_Register; }
  bool isImm() const override { return Kind == k_Immediate; }
  bool isMem() const override { return false; }

  SMLoc getStartLoc() const override { return Start; }
  SMLoc getEndLoc() const override { return End; }

  MCRegister getReg() const override {
    assert(Kind == k_Register && "not a register operand");
    return Reg.Reg;
  }

  StringRef getToken() const {
    assert(Kind == k_Token && "not a token operand");
    return StringRef(Tok.Data, Tok.Length);
  }

  const MCExpr *getImm() const {
    assert(Kind == k_Immediate && "not an immediate operand");
    return Imm.Val;
  }

  // Immediate range predicates — used by TableGen-generated AsmMatcher.

  // Return the constant value of an immediate operand, or nullopt for symbols.
  std::optional<int64_t> getImmVal() const {
    if (!isImm())
      return std::nullopt;
    const auto *CE = dyn_cast<MCConstantExpr>(Imm.Val);
    if (!CE)
      return std::nullopt; // symbol ref: defer to fixup
    return CE->getValue();
  }

  // For non-constant expressions (symbol refs, @l/@m/@h specifiers) the range
  // cannot be checked at parse time — accept and let the fixup handle it.
  // For constant expressions, enforce the field's valid range.
  bool isUImm6() const {
    if (!isImm())
      return false;
    auto V = getImmVal();
    return !V || isUInt<6>(*V);
  }
  bool isSImm6() const {
    if (!isImm())
      return false;
    auto V = getImmVal();
    return !V || isInt<6>(*V);
  }
  bool isUImm10() const {
    if (!isImm())
      return false;
    auto V = getImmVal();
    return !V || isUInt<10>(*V);
  }
  bool isUImm13() const {
    if (!isImm())
      return false;
    auto V = getImmVal();
    return !V || isUInt<13>(*V);
  }
  bool isUImm3() const {
    if (!isImm())
      return false;
    auto V = getImmVal();
    return !V || isUInt<3>(*V);
  }
  // Shift amount: 1..8.  No symbol refs are valid here; always a constant.
  bool isShiftImm8() const {
    auto V = getImmVal();
    return V && *V >= 1 && *V <= 8;
  }

  // MC emission helpers -----------------------------------------------------

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "invalid operand count");
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "invalid operand count");
    const MCExpr *Expr = getImm();
    if (const auto *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case k_Token:
      OS << "Token(" << getToken() << ")";
      break;
    case k_Register:
      OS << "Reg(" << getReg().id() << ")";
      break;
    case k_Immediate:
      OS << "Imm(";
      MAI.printExpr(OS, *getImm());
      OS << ")";
      break;
    }
  }
};

//===----------------------------------------------------------------------===//
// S1C33AsmParser
//===----------------------------------------------------------------------===//

class S1C33AsmParser : public MCTargetAsmParser {
  MCAsmParser &Parser;

#define GET_ASSEMBLER_HEADER
#include "S1C33GenAsmMatcher.inc"

public:
  // Target-specific operand diagnostic types (generated from AsmOperandClass
  // DiagnosticType entries).  Must be public and in class scope for
  // S1C33AsmParser::Match_InvalidXxx to resolve correctly.
  enum {
    FIRST_DIAG = MCTargetAsmParser::FIRST_TARGET_MATCH_RESULT_TY,
#define GET_OPERAND_DIAGNOSTIC_TYPES
#include "S1C33GenAsmMatcher.inc"
  };

private:
  // Parse a '%'-prefixed register token and return the MCRegister.
  // Consumes the '%' and the identifier.
  // Returns MCRegister() on failure (does not advance on failure).
  MCRegister tryParseRegName(SMLoc &StartLoc, SMLoc &EndLoc);

  // Parse a single operand or bracket group and append to Operands.
  bool parseOperandItem(OperandVector &Operands);

  // Parse an immediate (integer or expression) and append to Operands.
  bool parseImmediate(OperandVector &Operands);

public:
  S1C33AsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                 const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), Parser(Parser) {
    MCAsmParserExtension::Initialize(Parser);
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  // Allow R8 and R9 as GR32 operands in hand-written assembly.
  // These registers are reserved for register allocation but can be used
  // explicitly in hand-written assembly (e.g., as R9 for scratch in musfast.s).
  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override;

  ParseStatus parseDirective(AsmToken DirectiveID) override;

  MCAsmParser &getParser() const { return Parser; }
  AsmLexer &getLexer() const { return Parser.getLexer(); }
};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Auto-generated register matcher
//===----------------------------------------------------------------------===//

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "S1C33GenAsmMatcher.inc"

//===----------------------------------------------------------------------===//
// Register parsing
//===----------------------------------------------------------------------===//

MCRegister S1C33AsmParser::tryParseRegName(SMLoc &StartLoc, SMLoc &EndLoc) {
  // Caller must have already seen AsmToken::Percent.
  StartLoc = getLexer().getLoc();

  if (!getLexer().is(AsmToken::Percent))
    return MCRegister();

  SMLoc PercentLoc = getLexer().getLoc();
  Parser.Lex(); // eat '%'

  if (!getLexer().is(AsmToken::Identifier)) {
    // Not an identifier after '%': put back conceptually.
    // We cannot unlex, so return failure — the caller must not have consumed.
    // (Callers check for Percent before calling, so this is safe.)
    return MCRegister();
  }

  StringRef Name = getLexer().getTok().getString();
  EndLoc = getLexer().getTok().getEndLoc();
  MCRegister Reg = MatchRegisterName(Name);
  if (!Reg) {
    // Not a valid register name — report and bail.
    // (We already consumed '%'; emit error at the percent loc.)
    Error(PercentLoc, "unknown register name '%" + Name + "'");
    return MCRegister();
  }
  Parser.Lex(); // eat register name
  return Reg;
}

bool S1C33AsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                   SMLoc &EndLoc) {
  Reg = tryParseRegName(StartLoc, EndLoc);
  return !Reg;
}

ParseStatus S1C33AsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                             SMLoc &EndLoc) {
  if (!getLexer().is(AsmToken::Percent))
    return ParseStatus::NoMatch;
  Reg = tryParseRegName(StartLoc, EndLoc);
  if (!Reg)
    return ParseStatus::NoMatch;
  return ParseStatus::Success;
}

//===----------------------------------------------------------------------===//
// Operand parsing helpers
//===----------------------------------------------------------------------===//

bool S1C33AsmParser::parseImmediate(OperandVector &Operands) {
  SMLoc S = getLexer().getLoc();
  const MCExpr *Expr;
  if (getParser().parseExpression(Expr))
    return true;
  // @l/@m/@h specifiers are parsed automatically by MCAsmParser via
  // initializeAtSpecifiers in S1C33MCAsmInfo; no special handling needed here.
  SMLoc E = SMLoc::getFromPointer(getLexer().getLoc().getPointer() - 1);
  Operands.push_back(S1C33Operand::CreateImm(Expr, S, E));
  return false;
}

bool S1C33AsmParser::parseOperandItem(OperandVector &Operands) {
  SMLoc S = getLexer().getLoc();

  // --- '[' bracket group ---
  if (getLexer().is(AsmToken::LBrac)) {
    SMLoc BrS = S;
    Parser.Lex(); // eat '['
    Operands.push_back(S1C33Operand::CreateToken("[", BrS));

    if (!getLexer().is(AsmToken::Percent))
      return Error(getLexer().getLoc(), "expected '%' after '['");

    SMLoc PercentLoc = getLexer().getLoc();
    Parser.Lex(); // eat '%'

    if (!getLexer().is(AsmToken::Identifier))
      return Error(getLexer().getLoc(), "expected register name after '%'");

    StringRef Name = getLexer().getTok().getString();

    if (Name == "sp") {
      // Could be [%sp+imm] or [%sp]
      Parser.Lex(); // eat 'sp'
      if (getLexer().is(AsmToken::Plus)) {
        // [%sp+imm6] SP-relative
        SMLoc SpPlusLoc = PercentLoc;
        Parser.Lex(); // eat '+'
        // Build a stable string for "%sp+" on the side-channel string pool.
        // We use a static literal; the operand is short-lived.
        static const char SpPlusBuf[] = "%sp+";
        Operands.push_back(
            S1C33Operand::CreateToken(StringRef(SpPlusBuf, 4), SpPlusLoc));
        // Parse the immediate offset
        if (parseImmediate(Operands))
          return true;
      } else {
        // [%sp] — no offset
        static const char SpBuf[] = "%sp";
        Operands.push_back(
            S1C33Operand::CreateToken(StringRef(SpBuf, 3), PercentLoc));
      }
    } else {
      // [%rb] register indirect.
      MCRegister Reg = MatchRegisterName(Name);
      if (!Reg)
        return Error(PercentLoc, "unknown register name '%" + Name + "'");
      SMLoc RegS = PercentLoc;
      SMLoc RegE = getLexer().getTok().getEndLoc();
      Parser.Lex(); // eat register name
      Operands.push_back(S1C33Operand::CreateReg(Reg, RegS, RegE));
    }

    // Expect ']'
    if (!getLexer().is(AsmToken::RBrac))
      return Error(getLexer().getLoc(), "expected ']'");
    SMLoc BrE = getLexer().getLoc();
    Parser.Lex(); // eat ']'
    Operands.push_back(S1C33Operand::CreateToken("]", BrE));

    // Check for post-increment: [%rb]+ syntax
    if (getLexer().is(AsmToken::Plus)) {
      SMLoc PlusLoc = getLexer().getLoc();
      Parser.Lex(); // eat '+'
      static const char PlusBuf[] = "+";
      Operands.push_back(
          S1C33Operand::CreateToken(StringRef(PlusBuf, 1), PlusLoc));
    }
    return false;
  }

  // --- '%' register (or special token like %sp, %alr, %ahr) ---
  if (getLexer().is(AsmToken::Percent)) {
    SMLoc PercentLoc = S;
    Parser.Lex(); // eat '%'

    if (!getLexer().is(AsmToken::Identifier))
      return Error(getLexer().getLoc(), "expected register name after '%'");

    StringRef Name = getLexer().getTok().getString();
    SMLoc NameEnd = getLexer().getTok().getEndLoc();
    Parser.Lex(); // eat register name

    // Special registers that appear as literal tokens in assembly strings
    // (not as GR32 register operands in the matcher tables).
    if (Name == "sp" || Name == "alr" || Name == "ahr" || Name == "psr" ||
        Name == "pc") {
      // Build the token string "%sp", "%alr", etc.
      // Store in a side allocation that outlives this call.
      // For short names, use a static table.
      static const std::string SpecialNames[] = {"%sp", "%alr", "%ahr", "%psr",
                                                 "%pc"};
      static const StringRef SpecialKeys[] = {"sp", "alr", "ahr", "psr", "pc"};
      for (unsigned I = 0; I < 5; ++I) {
        if (Name == SpecialKeys[I]) {
          Operands.push_back(
              S1C33Operand::CreateToken(SpecialNames[I], PercentLoc));
          return false;
        }
      }
    }

    // GR32 general-purpose register
    MCRegister Reg = MatchRegisterName(Name);
    if (!Reg)
      return Error(PercentLoc, "unknown register '%" + Name + "'");
    Operands.push_back(S1C33Operand::CreateReg(Reg, PercentLoc, NameEnd));
    return false;
  }

  // --- Immediate or label ---
  return parseImmediate(Operands);
}

//===----------------------------------------------------------------------===//
// parseInstruction
//===----------------------------------------------------------------------===//

bool S1C33AsmParser::parseInstruction(ParseInstructionInfo &Info,
                                      StringRef Name, SMLoc NameLoc,
                                      OperandVector &Operands) {
  Operands.push_back(S1C33Operand::CreateToken(Name, NameLoc));

  if (getLexer().is(AsmToken::EndOfStatement)) {
    Parser.Lex(); // eat EndOfStatement
    return false;
  }

  // Parse comma-separated operands.
  while (true) {
    if (parseOperandItem(Operands)) {
      Parser.eatToEndOfStatement();
      return true;
    }

    if (!getLexer().is(AsmToken::Comma))
      break;
    Parser.Lex(); // eat ','
  }

  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    SMLoc Loc = getLexer().getLoc();
    Parser.eatToEndOfStatement();
    return Error(Loc, "unexpected token in instruction operands");
  }
  Parser.Lex(); // eat EndOfStatement
  return false;
}

//===----------------------------------------------------------------------===//
// matchAndEmitInstruction
//===----------------------------------------------------------------------===//

bool S1C33AsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned & /*Opcode*/,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned Result =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);

  switch (Result) {
  case Match_Success:
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, *STI);
    return false;

  case Match_MissingFeature:
    return Error(IDLoc,
                 "instruction requires a CPU feature not currently enabled");

  case Match_MnemonicFail:
    return Error(IDLoc, "unrecognized instruction mnemonic");

  case Match_InvalidOperand: {
    SMLoc ErrorLoc = IDLoc;
    if (ErrorInfo != ~0ULL) {
      if (ErrorInfo < Operands.size())
        ErrorLoc = ((S1C33Operand &)*Operands[ErrorInfo]).getStartLoc();
      else
        return Error(IDLoc, "too few operands for instruction");
    }
    return Error(ErrorLoc, "invalid operand for instruction");
  }

  case Match_InvalidUImm6:
    return Error(((S1C33Operand &)*Operands[ErrorInfo]).getStartLoc(),
                 "immediate must be an integer in the range [0, 63]");
  case Match_InvalidSImm6:
    return Error(((S1C33Operand &)*Operands[ErrorInfo]).getStartLoc(),
                 "immediate must be an integer in the range [-32, 31]");
  case Match_InvalidUImm10:
    return Error(((S1C33Operand &)*Operands[ErrorInfo]).getStartLoc(),
                 "immediate must be an integer in the range [0, 1023]");
  case Match_InvalidUImm13:
    return Error(((S1C33Operand &)*Operands[ErrorInfo]).getStartLoc(),
                 "immediate must be an integer in the range [0, 8191]");
  case Match_InvalidUImm3:
    return Error(((S1C33Operand &)*Operands[ErrorInfo]).getStartLoc(),
                 "immediate must be an integer in the range [0, 7]");
  case Match_InvalidShiftImm8:
    return Error(((S1C33Operand &)*Operands[ErrorInfo]).getStartLoc(),
                 "shift amount must be an integer in the range [1, 8]");

  default:
    return true;
  }
}

//===----------------------------------------------------------------------===//
// validateTargetOperandClass
//===----------------------------------------------------------------------===//

unsigned S1C33AsmParser::validateTargetOperandClass(MCParsedAsmOperand &AsmOp,
                                                    unsigned Kind) {
  // Allow R8 and R9 to be used as GR32 operands in hand-written assembly.
  // They are excluded from GR32 for register allocation but valid physically.
  S1C33Operand &Op = static_cast<S1C33Operand &>(AsmOp);
  if (Kind == MCK_GR32 && Op.isReg()) {
    MCRegister Reg = Op.getReg();
    if (Reg == S1C33::R8 || Reg == S1C33::R9)
      return Match_Success;
  }
  return Match_InvalidOperand;
}

//===----------------------------------------------------------------------===//
// Directive handling
//===----------------------------------------------------------------------===//

ParseStatus S1C33AsmParser::parseDirective(AsmToken DirectiveID) {
  StringRef Name = DirectiveID.getString();

  // .endfile — EPSON as33/pp33 end-of-file marker.  Treated as a no-op;
  // the LLVM assembler stops at actual end-of-input.
  if (Name == ".endfile") {
    Parser.eatToEndOfStatement();
    return ParseStatus::Success;
  }

  return ParseStatus::NoMatch;
}

//===----------------------------------------------------------------------===//
// Registration
//===----------------------------------------------------------------------===//

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeS1C33AsmParser() {
  RegisterMCAsmParser<S1C33AsmParser> X(getTheS1C33Target());
}
