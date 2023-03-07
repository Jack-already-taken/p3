# Target library
lib := libfs.a

## TODO: Phase 1
objs    := disk.o fs.o
CC      := gcc
CFLAGS  := -Wall -Wextra -Werror
CFLAGS  += -g

## TODO: Phase 1
ifneq ($(V),1)
Q = @
endif

# Dep tracking *must* be below the 'all' rule
deps := $(patsubst %.o, %.d, $(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

all: $(lib)

libfs.a: $(objs) 
	$(Q)ar rcs $@ $^

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)


clean:
	@echo "clean"
	$(Q)rm -f $(lib) $(objs) $(deps)