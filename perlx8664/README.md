# perlx8664
Helper functions to statically analyze obfuscated x86_64 code where functions are chopped into
chunks connected via 'jmpq'. In addition tries to deobfuscate code that use runtime
controlflow obfuscation via test, conditionally push function address and later "jmpq *-8(%rsp)". This code tries to reassemble a function, starting from the entry point and following
the instruction stream while trying to reason about the stack content. "objdump --start-address" is used for decode.
