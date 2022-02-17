## Makefile

WARN = -Wall -Wextra -Wmissing-prototypes -Wshadow -Wconversion
WARN += -Wno-long-long -Wno-unused-parameter
WARN += -pedantic

all:: libyasve.so

libyasve.so::
	cc -std=gnu99 -fPIC -shared -DDEBUG -g $(WARN) \
	    -Wl,-soname,libyasve.so -o libyasve.so yasve.c

libyasve.so-static::
	cc -std=gnu99 -fPIC -shared -DDEBUG -O2 -g $(WARN) \
	    -Wl,-soname,libyasve.so -o libyasve.so yasve.c preloader.c

runstatic:: runstatic.c libyasve.so-static
	cc -std=gnu99 -DDEBUG -g $(WARN) -o runstatic runstatic.c libyasve.so -lelf -lm

insn.c::
	sh -x ./make-insn-table.sh

test00::
	gcc -march=armv8.2-a+sve -Ofast -g test00.c
test01::
	gcc -march=armv8.2-a+sve -Ofast -g test01.c -lm
test02::
	gcc -march=armv8.2-a+sve -Ofast -g test02.c

clean::
	rm -f a.out runstatic *.o *.so core.* *.s
