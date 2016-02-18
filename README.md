# cavecarver
Use .gnu.version section  in an elf executable as container for custom code.

# Rational
If you want to patch a .so in Linux to inject custom code
the virtual address space layout should be kept unchanged.
Inserting new code in the middle of the .text section requires
patching .rel, .rela, patching functions pointers [.fini,.init].array,
it requires going through all code and patching pc relative code in sections.
This in turn requires a complete analysis of the callgraph of a program
because only that way the bytealigned x86_64 code can be decoded.

To avoid all this there are these "easy" possibilities:

 - Discover a section that can be reused: The .gnu.version section
   is a good candidate. To do so, first remove DT_VERNEED, DT_VERNEEDNUM
   and DT_VERSYM entries in the .dynamic section. Then remark the .gen.version as PROGBIT.
   The .gnu.version is normally mapped into the first program header specified section
   and is executable. If you need RW space to save data to write over the .comment section,
   or you can increase the .bss via the program header.

 - Another possible "cave" is the virtual address alignment space between the
   2 program header mapping, .text and .data, that normally exist. If this
   alignment exist we can append code at the end of the last section mapped
   by the executable program header. We have to adjust the section sizes and offsets
   and increase the program header attributes, but the virtual address space
   doesnt change.

 - As a last resort prepending to the beginning of the .init section mapped
   to the beginning of the text program header might be feasible. Pc relative
   jumps will still work, however the virtual address space will shift. because
   .so files are -fPIC compiled this can be fixed by "only" patching the .rel and .rela
   sections.

# Code
The file payload.cpp is a sample code that scans for the .gnu.version section and
writes over its content. It searches for the .dynamic section and removes the
versioning references. The code is inspired by: http://vxheaven.org/lib/var00.html
