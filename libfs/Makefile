# Target library
lib := libfs.a
objs    := disk.o fs.o
CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -MMD
LDFLAGS := -lc

ifneq ($(V),1)
Q = @
endif

ifneq ($(D),1)
CFLAGS	+= -O2
else
CFLAGS	+= -g
endif

all: $(lib)

deps := $(patsubst %.o, %.d, $(objs))
-include $(deps)

$(lib): $(objs)
	@echo "CC $@"
	$(Q) ar rcs $(LDFLAGS) $@ $^

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)


clean:
	@echo "clean"
	$(Q)rm -f $(lib) $(objs) $(deps)