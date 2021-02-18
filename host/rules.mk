
VPATH := $(src)
libs := $(lib:%=%.so)
override CFLAGS += $(cflags)
override LDFLAGS += $(ldflags)

subdirs := $(sort $(filter-out ./,$(dir $(foreach bin,$(bin) $(lib),$($(bin)-obj)))))
$(if $(subdirs),$(shell mkdir -p $(subdirs)))

all: $(libs) $(bin)
ifneq ($(kmodule),)
	$(MAKE) -C $(KERNEL_BUILD) M=$(CURDIR) KBUILD_EXTMOD_SRC=$(src) modules $(kmodule_env)
endif

define bld_obj

$1: %.o: %.c
	$(CC) -c -o $$@ $$< $(CFLAGS) $2

sinclude $(1:%.o=%.d)

$(1:%.o=%.d): %.d: %.c
	$(CC) -MM -MT $$@ $$< $(CFLAGS) $2 | sed 's/\(\S\+\)\.d[ :]*/\1.o \1.d : /g' > $$@

endef

__dummy := $(foreach lib,$(lib),$(eval $(call bld_obj,$(or $($(lib)-obj),$(lib).o),-fPIC $($(lib)-cflags))))
__dummy := $(foreach bin,$(bin),$(eval $(call bld_obj,$(or $($(bin)-obj),$(bin).o),$($(bin)-cflags))))

.SECONDEXPANSION:

ifneq ($(lib),)
$(libs): $$(or $$($$(patsubst %.so,%,$$@)-obj),$$(patsubst %.so,%,$$@).o)
	$(CC) --shared -o $@ $^ $(LDFLAGS) $($(@:%.so=%)-ldflags)
endif

ifneq ($(bin),)
$(bin): $(libs) $$(or $$($$@-obj),$$@.o)
	$(CC) -o $@ $^ $(LDFLAGS) $($@-ldflags)
endif

