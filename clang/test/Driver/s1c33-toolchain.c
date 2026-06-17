// Check toolchain selection for the two s1c33 triples: s1c33-none-piece
// selects the P/ECE toolchain (kernel callback runtime, P/ECE default
// libraries); s1c33-none-elf selects the generic bare-metal toolchain.

// RUN: %clang -### --target=s1c33-none-piece %s 2>&1 \
// RUN:   | FileCheck --check-prefix=PIECE %s
// PIECE: "-Bstatic"
// PIECE-SAME: "-m" "elf32ls1c33"
// PIECE-SAME: "crt0.o"
// PIECE-SAME: "-lclang_rt.builtins-s1c33"
// PIECE-SAME: "--start-group" "-lcxxrt" "-lpceapi" "-lpicortt" "-lpceshim" "-lc" "-lm" "--end-group"

// -mprintf=/-mscanf= select a picolibc printf/scanf variant via a linker
// --defsym alias of vfprintf/vfscanf, emitted ahead of the libraries.
// RUN: %clang -### --target=s1c33-none-piece -mprintf=integer -mscanf=float %s 2>&1 \
// RUN:   | FileCheck --check-prefix=MPRINTF %s
// MPRINTF: "--defsym=vfprintf=__i_vfprintf"
// MPRINTF-SAME: "--defsym=vfscanf=__f_vfscanf"
// MPRINTF-SAME: "-lpicortt"

// An invalid -mprintf= value is rejected.
// RUN: not %clang -### --target=s1c33-none-piece -mprintf=bogus %s 2>&1 \
// RUN:   | FileCheck --check-prefix=MPRINTF-BAD %s
// MPRINTF-BAD: invalid value 'bogus' in '-mprintf=bogus'

// The default linker is ld.lld (inherited from the bare-metal toolchain).
// RUN: %clang -### --target=s1c33-none-piece %s 2>&1 \
// RUN:   | FileCheck --check-prefix=PIECE-LLD %s
// PIECE-LLD: ld.lld

// s1c33-none-elf must NOT link any P/ECE library.
// RUN: %clang -### --target=s1c33-none-elf %s 2>&1 \
// RUN:   | FileCheck --check-prefix=BAREMETAL %s
// BAREMETAL: "-Bstatic"
// BAREMETAL-SAME: "-m" "elf32ls1c33"
// BAREMETAL-NOT: "-lpceapi"
// BAREMETAL-NOT: "-lpceshim"
// BAREMETAL-NOT: "-lcxxrt"

// -nostdlib suppresses the P/ECE default libraries and startup files.
// RUN: %clang -### --target=s1c33-none-piece -nostdlib %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOSTDLIB %s
// NOSTDLIB-NOT: "crt0.o"
// NOSTDLIB-NOT: "-lpceapi"

// -nostartfiles suppresses only the startup files, not the libraries.
// RUN: %clang -### --target=s1c33-none-piece -nostartfiles %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOSTART %s
// NOSTART-NOT: "crt0.o"
// NOSTART-NOT: "crti.o"
// NOSTART: "-lpceapi"

// With a sysroot: --sysroot is forwarded to the linker, crt0.o/crti.o are
// resolved inside <sysroot>/lib, and the default linker script piece.ld is
// injected.  crti.o and piece.ld are added only if they exist in the tree.
// RUN: %clang -### --target=s1c33-none-piece \
// RUN:     --sysroot=%S/Inputs/basic_piece_tree %s 2>&1 \
// RUN:   | FileCheck --check-prefix=SYSROOT %s
// SYSROOT: "--sysroot={{[^"]*}}basic_piece_tree"
// SYSROOT: "{{[^"]*}}basic_piece_tree{{/|\\\\}}lib{{/|\\\\}}crt0.o"
// SYSROOT-SAME: "{{[^"]*}}basic_piece_tree{{/|\\\\}}lib{{/|\\\\}}crti.o"
// SYSROOT-SAME: "-T{{[^"]*}}basic_piece_tree{{/|\\\\}}lib{{/|\\\\}}piece.ld"

// A user -T suppresses the default piece.ld injection.
// RUN: %clang -### --target=s1c33-none-piece \
// RUN:     --sysroot=%S/Inputs/basic_piece_tree -T custom.ld %s 2>&1 \
// RUN:   | FileCheck --check-prefix=USERT %s
// USERT-NOT: piece.ld
// USERT: "-T" "custom.ld"
// USERT-NOT: piece.ld

// Without a sysroot, neither crti.o nor piece.ld can be found, so they
// must not appear on the link line (crt0.o stays as a bare name).
// RUN: %clang -### --target=s1c33-none-piece %s 2>&1 \
// RUN:   | FileCheck --check-prefix=NOSYSROOT %s
// NOSYSROOT-NOT: "crti.o"
// NOSYSROOT-NOT: piece.ld
