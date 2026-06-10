// Check S1C33 target macros, including the P/ECE platform macros that are
// defined only for the s1c33-*-piece triple.

// RUN: %clang_cc1 -triple s1c33-none-piece -dM -E %s \
// RUN:   | FileCheck --check-prefix=PIECE %s
// PIECE-DAG: #define __PIECE__ 1
// PIECE-DAG: #define __piece__ 1
// PIECE-DAG: #define __S1C33__ 1
// PIECE-DAG: #define __s1c33 1
// PIECE-DAG: #define __s1c33__ 1

// RUN: %clang_cc1 -triple s1c33-none-elf -dM -E %s \
// RUN:   | FileCheck --check-prefix=BM %s
// RUN: %clang_cc1 -triple s1c33-none-elf -dM -E %s \
// RUN:   | FileCheck --check-prefix=BM-NOPIECE %s
// BM-DAG: #define __S1C33__ 1
// BM-DAG: #define __s1c33 1
// BM-DAG: #define __s1c33__ 1
// BM-NOPIECE-NOT: #define __PIECE__
// BM-NOPIECE-NOT: #define __piece__
