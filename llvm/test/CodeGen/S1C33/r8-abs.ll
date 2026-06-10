; RUN: llc -mtriple=s1c33-none-piece -o - %s | FileCheck %s --check-prefixes=CHECK,R8ABS
; RUN: llc -mtriple=s1c33-none-piece -mattr=-r8-abs -o - %s | FileCheck %s --check-prefixes=CHECK,NO_R8ABS
; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s --check-prefixes=CHECK,NO_R8ABS
; RUN: llc -mtriple=s1c33-none-elf -mattr=+r8-abs -o - %s | FileCheck %s --check-prefixes=CHECK,R8ABS
;
; R8 absolute addressing for global load/store.  When FeatureR8AbsGlobal is
; on, access to a wrapped global address is emitted as
;   ext sym@ah / ext sym@al / ld.* [%r8]     (6 bytes)
; taking advantage of R8 being held at 0 by the P/ECE kernel.  With the
; feature disabled, the access falls back to the materialize-then-indirect
; path:
;   ext sym@h / ext sym@m / ld.w %rd, sym@l
;   ld.* [%rd], ...                           (8 bytes)
;
; R8 == 0 is an OS guarantee, not a CPU property, so the feature defaults to
; on only for s1c33-*-piece triples; bare-metal s1c33-none-elf must opt in
; with -mattr=+r8-abs (and set R8 = 0 in its startup code).
;
; Compatible with gcc33's @ah/@al syntax.  The ELF relocations are
; R_S1C33_REL_AH / R_S1C33_REL_AL (the "REL_" prefix is historical — the
; effective semantics is ABSOLUTE 26-bit).

@gw = external global i32
@gb = external global i8
@gh = external global i16

; CHECK-LABEL: load_word:
; R8ABS:       ext gw@ah
; R8ABS-NEXT:  ext gw@al
; R8ABS-NEXT:  ld.w %r10, [%r8]
; R8ABS-NEXT:  ret
; NO_R8ABS:      ext gw@h
; NO_R8ABS-NEXT: ext gw@m
; NO_R8ABS-NEXT: ld.w %r{{[0-9]+}}, gw@l
; NO_R8ABS-NEXT: ld.w %r10, [%r{{[0-9]+}}]
; NO_R8ABS-NEXT: ret
define i32 @load_word() {
  %v = load i32, ptr @gw
  ret i32 %v
}

; CHECK-LABEL: store_word:
; R8ABS:       ext gw@ah
; R8ABS-NEXT:  ext gw@al
; R8ABS-NEXT:  ld.w [%r8], %r12
; R8ABS-NEXT:  ret
define void @store_word(i32 %v) {
  store i32 %v, ptr @gw
  ret void
}

; CHECK-LABEL: load_byte_sext:
; R8ABS:       ext gb@ah
; R8ABS-NEXT:  ext gb@al
; R8ABS-NEXT:  ld.b %r10, [%r8]
; R8ABS-NEXT:  ret
define i32 @load_byte_sext() {
  %v = load i8, ptr @gb
  %e = sext i8 %v to i32
  ret i32 %e
}

; CHECK-LABEL: load_byte_zext:
; R8ABS:       ext gb@ah
; R8ABS-NEXT:  ext gb@al
; R8ABS-NEXT:  ld.ub %r10, [%r8]
; R8ABS-NEXT:  ret
define i32 @load_byte_zext() {
  %v = load i8, ptr @gb
  %e = zext i8 %v to i32
  ret i32 %e
}

; CHECK-LABEL: store_byte:
; R8ABS:       ext gb@ah
; R8ABS-NEXT:  ext gb@al
; R8ABS-NEXT:  ld.b [%r8], %r{{[0-9]+}}
; R8ABS:       ret
define void @store_byte(i8 %v) {
  store i8 %v, ptr @gb
  ret void
}

; CHECK-LABEL: load_half_sext:
; R8ABS:       ext gh@ah
; R8ABS-NEXT:  ext gh@al
; R8ABS-NEXT:  ld.h %r10, [%r8]
; R8ABS-NEXT:  ret
define i32 @load_half_sext() {
  %v = load i16, ptr @gh
  %e = sext i16 %v to i32
  ret i32 %e
}

; CHECK-LABEL: load_half_zext:
; R8ABS:       ext gh@ah
; R8ABS-NEXT:  ext gh@al
; R8ABS-NEXT:  ld.uh %r10, [%r8]
; R8ABS-NEXT:  ret
define i32 @load_half_zext() {
  %v = load i16, ptr @gh
  %e = zext i16 %v to i32
  ret i32 %e
}

; CHECK-LABEL: store_half:
; R8ABS:       ext gh@ah
; R8ABS-NEXT:  ext gh@al
; R8ABS-NEXT:  ld.h [%r8], %r{{[0-9]+}}
; R8ABS:       ret
define void @store_half(i16 %v) {
  store i16 %v, ptr @gh
  ret void
}

; GEP-style offset folding: (Wrapper (tga @arr, 40)) is matched the same way,
; producing `ext arr+40@ah / ext arr+40@al / ld.w %rd, [%r8]`.
%struct.S = type { [40 x i8], i32 }
@arr = external global %struct.S

; CHECK-LABEL: load_with_offset:
; R8ABS:       ext arr+40@ah
; R8ABS-NEXT:  ext arr+40@al
; R8ABS-NEXT:  ld.w %r10, [%r8]
; R8ABS-NEXT:  ret
define i32 @load_with_offset() {
  %p = getelementptr inbounds %struct.S, ptr @arr, i32 0, i32 1
  %v = load i32, ptr %p
  ret i32 %v
}
