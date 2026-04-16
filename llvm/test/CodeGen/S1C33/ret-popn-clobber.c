// RUN: clang -cc1 -triple s1c33-none-elf -O1 -S -o - %s | FileCheck %s

// Regression test for ret.d delay-slot filling across popn.
// popn %r3 restores R0-R3, so a return-value staging move that reads one of
// those registers must not be moved into the ret.d slot after popn.

extern int test_sp_1(int, int);
extern int test_sp_2(int, int);
extern int test_sp_3(int, int);
extern int test_sp_4(int, int);
extern int test_sp_5(int, int);
extern int test_sp_6(int, int);

// CHECK-LABEL: main:
// CHECK:       ld.w %r10, %r1
// CHECK-NEXT:  popn %r3
// CHECK-NEXT:  ret
int main(void) {
  int rc = 0;

  if (test_sp_1(-0x33333334, 0x80) != 0x80)
    rc = 1;
  if (test_sp_2(-0x33333334, 0x7f) != 0x7f)
    rc = 2;
  if (test_sp_3(-0x33333334, 0x13) != 0x13)
    rc = 3;
  if (test_sp_4(-0x33333334, 0x12) != 0x12)
    rc = 4;
  if (test_sp_5(-0x33333334, 0x11) != 0x11)
    rc = 5;
  if (test_sp_6(-0x33333334, 0x12) != 0x12)
    rc = 6;

  return rc;
}
