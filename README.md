# cavecarver
Use .gnu.version section  in an elf executable as container for custom code.

# Rational
If you want to patch a .so in linux the easiest way it to reuse the space thats
allocated by the .gnu.version section. To do so, first remove DT_VERNEED, DT_VERNEEDNUM
and DT_VERSYM entries in the .dynamic section. Then remark the .gen.version as PROGBIT.
The .gen.version is normally mapped into the first program header specified section
and is executable.
