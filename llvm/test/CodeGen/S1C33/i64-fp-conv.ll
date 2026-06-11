; RUN: llc -mtriple=s1c33-none-elf -o - %s | FileCheck %s
;
; Float/double <-> i64 conversion libcalls. EPSON's gcc33-era SDK never
; implemented these (__fixsfdi etc. are missing from fp.lib); they are
; provided by compiler-rt. This test pins the exact entry-point names the
; backend emits.
;
; Args are already in place per the i64/double register-pair ABI, so each
; function is a bare call + ret.

; CHECK-LABEL: f2sll:
; CHECK:       call __fixsfdi
define i64 @f2sll(float %x) {
  %r = fptosi float %x to i64
  ret i64 %r
}

; CHECK-LABEL: f2ull:
; CHECK:       call __fixunssfdi
define i64 @f2ull(float %x) {
  %r = fptoui float %x to i64
  ret i64 %r
}

; CHECK-LABEL: sll2f:
; CHECK:       call __floatdisf
define float @sll2f(i64 %x) {
  %r = sitofp i64 %x to float
  ret float %r
}

; CHECK-LABEL: ull2f:
; CHECK:       call __floatundisf
define float @ull2f(i64 %x) {
  %r = uitofp i64 %x to float
  ret float %r
}

; CHECK-LABEL: d2sll:
; CHECK:       call __fixdfdi
define i64 @d2sll(double %x) {
  %r = fptosi double %x to i64
  ret i64 %r
}

; CHECK-LABEL: d2ull:
; CHECK:       call __fixunsdfdi
define i64 @d2ull(double %x) {
  %r = fptoui double %x to i64
  ret i64 %r
}

; CHECK-LABEL: sll2d:
; CHECK:       call __floatdidf
define double @sll2d(i64 %x) {
  %r = sitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: ull2d:
; CHECK:       call __floatundidf
define double @ull2d(i64 %x) {
  %r = uitofp i64 %x to double
  ret double %r
}
