// C++ exceptions and RTTI are off by default on s1c33 (bare-metal target;
// the P/ECE runtime has no unwinder), following the XCore/PS4 pattern.
// Both s1c33 triples share the defaults; explicit flags override them.

// RUN: %clang -### --target=s1c33-none-elf -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=DEFAULT %s
// RUN: %clang -### --target=s1c33-none-piece -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=DEFAULT %s
// DEFAULT: "-cc1"
// DEFAULT-NOT: "-fcxx-exceptions"
// DEFAULT: "-fno-rtti"

// RUN: %clang -### --target=s1c33-none-piece -fexceptions -fcxx-exceptions \
// RUN:     -c %s 2>&1 | FileCheck --check-prefix=EXCEPTIONS %s
// EXCEPTIONS: "-fcxx-exceptions"

// RUN: %clang -### --target=s1c33-none-piece -frtti -c %s 2>&1 \
// RUN:   | FileCheck --check-prefix=RTTI %s
// RTTI-NOT: "-fno-rtti"
