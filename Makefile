# GNU Make workspace makefile autogenerated by Premake

ifndef config
  config=debug
endif

ifndef verbose
  SILENT = @
endif

ifeq ($(config),debug)
  PongBoy_config = debug
endif
ifeq ($(config),release)
  PongBoy_config = release
endif

PROJECTS := PongBoy

.PHONY: all clean help $(PROJECTS) 

all: $(PROJECTS)

PongBoy:
ifneq (,$(PongBoy_config))
	@echo "==== Building PongBoy ($(PongBoy_config)) ===="
	@${MAKE} --no-print-directory -C . -f PongBoy.make config=$(PongBoy_config)
endif

clean:
	@${MAKE} --no-print-directory -C . -f PongBoy.make clean

help:
	@echo "Usage: make [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "  debug"
	@echo "  release"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   PongBoy"
	@echo ""
	@echo "For more information, see http://industriousone.com/premake/quick-start"