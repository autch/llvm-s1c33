; RUN: llc -mtriple=s1c33-none-elf -O1 -o - %s | FileCheck %s
;
; LowerGlobalAddress splits nonzero offsets into a separate ADD so that
; multiple accesses to the same global (struct field accesses) share a common
; base via CSE.  Each field access then collapses to ext+Lxx_ri_off instead
; of ext+ext+ld.w for each distinct "sym+N" constant.
;
; When there is only one user the ADD is combined back so the single-access
; path still materializes the full address in one ext/ext/ld.w sequence.

%struct.S = type { [13 x i8], [25 x i8], i8, i32, i32 }
@arr = external global [0 x %struct.S]

; Multi-field store: base `arr` is materialized once; each field access becomes
; ext <offset> + st.w/st.b [%rb].  No "arr+38@h" / "arr+40@h" / "arr+44@h"
; relocations are emitted — only the base `arr@h`.
define void @setfields(i32 %i, i8 %iconf, i32 %length, i32 %adrs) {
; CHECK-LABEL: setfields:
; CHECK:       ext arr@h
; CHECK-NEXT:  ext arr@m
; CHECK-NEXT:  ld.w {{%r[0-9]+}}, arr@l
; CHECK-NOT:   arr+{{[0-9]+}}@
; CHECK:       ret
  %p = getelementptr inbounds %struct.S, ptr @arr, i32 %i
  %piconf = getelementptr inbounds nuw i8, ptr %p, i32 38
  store i8 %iconf, ptr %piconf
  %plen = getelementptr inbounds nuw i8, ptr %p, i32 40
  store i32 %length, ptr %plen
  %padr = getelementptr inbounds nuw i8, ptr %p, i32 44
  store i32 %adrs, ptr %padr
  ret void
}

; Single-use nonzero offset: offset is folded back into the wrapped target
; global address, producing a single ext/ext/ld.w arr+40@l sequence.
define i32 @load_one_field() {
; CHECK-LABEL: load_one_field:
; CHECK:       ext arr+40@h
; CHECK-NEXT:  ext arr+40@m
; CHECK-NEXT:  ld.w {{%r[0-9]+}}, arr+40@l
; CHECK-NEXT:  ld.w {{%r[0-9]+}}, [{{%r[0-9]+}}]
; CHECK-NEXT:  ret
  %p = getelementptr inbounds %struct.S, ptr @arr, i32 0, i32 3
  %v = load i32, ptr %p
  ret i32 %v
}
