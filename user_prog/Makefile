SUBDIRS = MeMView\
		  test\
		  Explorer\
		  Settings\
		  Terminal\
		  Game3D\
		  CLIHello\
		  AppStore

.PHONY: all libhx86 libmbedtls $(SUBDIRS) clean

all: libhx86 libmbedtls $(SUBDIRS)

# Build the libraries first
libhx86:
	@echo ""
	@echo "--- Building libhx86 Library ---"
	@$(MAKE) -C libhx86

libmbedtls: libhx86
	@echo ""
	@echo "--- Building mbedTLS Library ---"
	@$(MAKE) -C libmbedtls

# Build the apps
$(SUBDIRS): libhx86 libmbedtls
	@echo ""
	@echo "--- Building $@ ---"
	@$(MAKE) -C $@

clean:
	@$(MAKE) -C libhx86 clean
	@$(MAKE) -C libmbedtls clean
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
