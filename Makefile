CROSS_COMPILE   :=
AR              := $(CROSS_COMPILE)ar
CC              := $(CROSS_COMPILE)gcc

ARFLAGS   := rc
CFLAGS    := -g -O0
CPPFLAGS  := -std=gnu99 -Wall -Werror -I. -MMD
LDFLAGS   := -g -L$(CURDIR)
LIBS      := -lm

lib_src := linkedlist.c mathlib.c port_memory.c queue.c rbtree.c tree234.c memcache.c tzset.c hash_table.c
lib_obj := $(patsubst %.c,%.o,$(lib_src))
lib_dep := $(patsubst %.c,%.d,$(lib_src))
lib_out := libcommon.a

###
user_src := samples/tz.c
user_obj := $(patsubst %.c,%.o,$(user_src))
user_dep := $(patsubst %.c,%.d,$(user_src))
user_out := run.exe
###

define compile_ar
#$(AR)	$(ARFLAGS)
#$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
endef

all: $(user_out)

-include $(lib_dep) $(user_dep)
$(lib_out): $(lib_obj)
	$(AR) $(ARFLAGS) $@ $^
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(user_out): $(user_obj) $(lib_out)
	$(CC) $(LDFLAGS) -o $@ $< -lcommon $(LIBS)

clean:
	$(RM) $(lib_obj) $(user_obj)
	$(RM) $(lib_dep) $(user_dep)
	$(RM) $(lib_out) $(user_out)
