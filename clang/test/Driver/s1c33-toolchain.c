// Check toolchain selection for the two s1c33 triples: s1c33-none-piece
// selects the P/ECE toolchain (kernel callback runtime, P/ECE default
// libraries); s1c33-none-elf selects the generic bare-metal toolchain.

// RUN: %clang -### --target=s1c33-none-piece %s 2>&1 \
// RUN:   | FileCheck --check-prefix=PIECE %s
// PIECE: "-Bstatic"
// PIECE-SAME: "-m" "elf32ls1c33"
// PIECE-SAME: "crt0.o"
// PIECE-SAME: "-lclang_rt.builtins-s1c33"
// PIECE-SAME: "--start-group" "-lcxxrt" "-lpceapi" "-lpceshim" "-lc" "-lm" "--end-group"

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
