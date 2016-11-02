all: shpreload.so

PERL_CCOPTS=$(shell sh staticperl perl -MExtUtils::Embed -e ccopts)
PERL_LDOPTS=$(shell sh staticperl perl -MExtUtils::Embed -e ldopts)

#M32_OPT=-m32 
#M32_OPT=
shpreload.so: shpreload.c
	@echo "sh staticperl perl ccopts: $(PERL_CCOPTS)"
	@echo "sh staticperl perl ldopts: $(PERL_LDOPTS)"
	gcc $(M32_OPT) -DSHINTER_PERL5LIB="\"$(CURDIR)\"" $(PERL_CCOPTS) -fPIC -shared -g -o shpreload.so shpreload.c  -Wl,--version-script=exportmap -lm $(PERL_LDOPTS) -ldl

.PHONY: shpreload.so staticperl

staticperl:
	wget http://cvs.schmorp.de/App-Staticperl/bin/staticperl; 
	cp staticperl staticperl.tmp
	cat staticperl.tmp | sed -e 's/-g /-g -fPIC $(M32_OPT) /' \
                           | sed -e 's/PERL_LDFLAGS=/PERL_LDFLAGS="$(M32_OPT)" /' > staticperl
	chmod a+x staticperl
	sh staticperl clean
	sh staticperl build
	sh staticperl install


clean:
	rm staticperl
