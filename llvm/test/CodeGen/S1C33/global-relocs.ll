; RUN: llc -mtriple=s1c33-none-elf -filetype=obj -o %t %s
; RUN: llvm-objdump -d --triple=s1c33-none-elf %t | FileCheck %s --check-prefix=DIS
; RUN: llvm-objdump -r %t | FileCheck %s --check-prefix=RELOC
;
; Global variable access: address materialization with 3-level ext relaxation.
; Both local and external globals use EXT2 (ext+ext+ld.w) + 3 ABS_H/M/L relocs.
;
; Even a local global at section offset 0 has a non-absolute symbol whose final
; linked address (e.g. 0x10034c) does not fit in 6 bits.  The relaxation code
; correctly treats any non-absolute symbol as always requiring EXT2 so that the
; linker receives the full ABS_H/ABS_M/ABS_L triple to patch the 28-bit address.

@local_var = global i32 42
@external_var = external global i32

; Local global: non-absolute symbol → always relaxes to EXT2.
; DIS-LABEL: <load_local>:
; DIS:        ext
; DIS-NEXT:   ext
; DIS-NEXT:   ld.w
define i32 @load_local() {
  %v = load i32, ptr @local_var
  ret i32 %v
}

; External global: non-absolute symbol → EXT2 form.
; DIS-LABEL: <load_external>:
; DIS:        ext
; DIS-NEXT:   ext
; DIS-NEXT:   ld.w
define i32 @load_external() {
  %v = load i32, ptr @external_var
  ret i32 %v
}

; Both accesses emit ABS_H + ABS_M + ABS_L relocations.
; RELOC: local_var
; RELOC: external_var
