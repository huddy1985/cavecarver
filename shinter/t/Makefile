
all: preapre
	mkdir -p obj
	SH_INTERPOSER_LOG=$(CURDIR)/obj/def.log ./mygcc -o obj/obj.o obj.c 

preapre:
	cat ../shinterposer.sh | sed -e 's/\"ls\"/\".\/myls.real\"/'  > ./myls; \
	chmod a+rwx ./myls
	cat ../shinterposer.sh | sed -e 's/\"ls\"/\".\/mygcc.real\"/' > ./mygcc; \
	chmod a+rwx ./mygcc

preload:
	MYPRE=1 LD_PRELOAD=../shpreload.so bash -c "ls -la"
