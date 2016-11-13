SRC=\
decoder \
distorm \
instructions \
insts \
mnemonics \
operands \
prefix \
textdefs \
wstring

libdstorm.o: $(foreach i,$(SRC),src/$(i).o)
	gcc -static -static-libgcc -g -Wl,-r -o $@ -Wl,--version-script=exportmap  $^

#-Wl,-Bdynamic,-lgcc_s,-Bstatic

src/%.o: src/%.c
	gcc -c -fPIC -g $< -o $@

#-fno-builtin

.PHONY: libdstorm.a

clean:
	-rm src/*.o libdstorm.o
