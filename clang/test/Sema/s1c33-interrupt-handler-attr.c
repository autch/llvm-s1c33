// RUN: %clang_cc1 %s -triple s1c33-none-elf -verify -fsyntax-only
// RUN: %clang_cc1 %s -triple s1c33-none-piece -verify -fsyntax-only

struct a { int b; };

struct a test __attribute__((interrupt_handler)); // expected-warning {{'interrupt_handler' attribute only applies to functions}}

__attribute__((interrupt_handler(12))) void foo(void) {} // expected-error {{'interrupt_handler' attribute takes no arguments}}

__attribute__((interrupt_handler)) int fooa(void) { return 0; } // expected-warning {{'interrupt_handler' attribute only applies to functions that have a 'void' return type}}

__attribute__((interrupt_handler)) void foob(int a) {} // expected-warning {{'interrupt_handler' attribute only applies to functions that have no parameters}}

__attribute__((interrupt_handler)) void fooc(void) {}

__attribute__((interrupt_handler)) void food() {}
