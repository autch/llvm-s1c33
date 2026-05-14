// RUN: %clang_cc1 -triple s1c33-none-elf -emit-llvm -o - %s | FileCheck %s
//
// S5U1C33000C (gcc33) quirk, DESIGN_SPEC §3.5: a "single-element struct" of
// <= 32 bits is passed in an argument register like its scalar element, not
// on the stack like every other struct.  clang coerces it to an i8/i16/i32
// marked `inreg`; the backend high-bit-packs the 8/16-bit forms.  Multi-
// element structs, unions with more than one member, and anything larger
// than 32 bits stay entirely on the stack (byval).

struct s8   { char  a; };
struct s16  { short a; };
struct s32  { int   a; };
struct sptr { void *a; };
struct nested { struct s16 inner; };   // single-element through nesting
struct multi  { char a, b; };          // two members -> not single-element
struct big    { int a, b; };           // 64 bits -> too large

// CHECK-LABEL: define {{.*}}void @take_s8(i8 inreg
void take_s8(struct s8 v) { (void)v; }

// CHECK-LABEL: define {{.*}}void @take_s16(i16 inreg
void take_s16(struct s16 v) { (void)v; }

// CHECK-LABEL: define {{.*}}void @take_s32(i32 inreg
void take_s32(struct s32 v) { (void)v; }

// CHECK-LABEL: define {{.*}}void @take_sptr(i32 inreg
void take_sptr(struct sptr v) { (void)v; }

// CHECK-LABEL: define {{.*}}void @take_nested(i16 inreg
void take_nested(struct nested v) { (void)v; }

// A multi-element struct stays on the stack: passed byval, never inreg.
// CHECK-LABEL: define {{.*}}void @take_multi(
// CHECK-SAME: byval(%struct.multi)
// CHECK-NOT: inreg
void take_multi(struct multi v) { (void)v; }

// A struct larger than 32 bits stays on the stack.
// CHECK-LABEL: define {{.*}}void @take_big(
// CHECK-SAME: byval(%struct.big)
// CHECK-NOT: inreg
void take_big(struct big v) { (void)v; }

// A scalar i32 argument that is NOT a struct must not gain `inreg`.
// CHECK-LABEL: define {{.*}}void @take_int(i32 noundef %v)
void take_int(int v) { (void)v; }
