TOP = ../../../..
include $(TOP)/configs/current

INCLUDE_DIRS = \
	-I$(TOP)/include \
	-I$(TOP)/src/mesa \
	-I$(TOP)/src/mesa/main \
	-I$(TOP)/src/gallium/include \
	-I$(TOP)/src/gallium/drivers \
	-I$(TOP)/src/gallium/auxiliary

PSL1GHT_WINSYS_SOURCES = psl1ght_winsys.c

PSL1GHT_WINSYS_OBJECTS = $(PSL1GHT_WINSYS_SOURCES:.c=.o)

NOUVEAU_SOURCES = nouveau/nouveau_bo.c nouveau/nouveau_channel.c	\
	nouveau/nouveau_grobj.c nouveau/nouveau_notifier.c		\
	nouveau/nouveau_pushbuf.c nouveau/nouveau_reloc.c		\
	nouveau/nouveau_resource.c

NOUVEAU_OBJECTS = $(NOUVEAU_SOURCES:.c=.o)

LIBS = \
	$(TOP)/src/gallium/winsys/sw/psl1ght/libpsl1ght.a \
	$(TOP)/src/gallium/drivers/nvfx/libnvfx.a \
	$(TOP)/src/gallium/drivers/nouveau/libnouveau.a \
	$(TOP)/src/gallium/drivers/softpipe/libsoftpipe.a \
	$(TOP)/src/gallium/drivers/trace/libtrace.a \
	$(TOP)/src/mapi/glapi/libglapi.a \
	$(TOP)/src/mesa/libmesagallium.a \
	$(GALLIUM_AUXILIARIES)

.c.o:
	$(CC) -c $(INCLUDE_DIRS) $(DEFINES) $(CFLAGS) $< -o $@

default: $(TOP)/$(LIB_DIR)/$(GL_LIB_NAME)

# Make the libGL.a library
$(TOP)/$(LIB_DIR)/$(GL_LIB_NAME): $(PSL1GHT_WINSYS_OBJECTS) $(NOUVEAU_OBJECTS) $(LIBS) Makefile
	$(TOP)/bin/mklib -o $(GL_LIB) \
		-linker "$(CC)" \
		-install $(TOP)/$(LIB_DIR) \
		$(MKLIB_OPTIONS) $(PSL1GHT_WINSYS_OBJECTS) $(NOUVEAU_OBJECTS) $(LIBS)

depend: $(PSL1GHT_WINSYS_SOURCES) $(NOUVEAU_SOURCES)
	@ echo "running $(MKDEP)"
	@ rm -f depend  # workaround oops on gutsy?!?
	@ touch depend
	$(MKDEP) $(MKDEP_OPTIONS) $(DEFINES) $(INCLUDE_DIRS) $(PSL1GHT_WINSYS_SOURCES) $(NOUVEAU_SOURCES) \
		> /dev/null 2>/dev/null

install: default
	$(INSTALL) -d $(INSTALL_DIR)/include/GL
	$(INSTALL) -d $(INSTALL_DIR)/$(LIB_DIR)
	$(INSTALL) -m 644 $(TOP)/include/GL/*.h $(INSTALL_DIR)/include/GL
	@if [ -e $(TOP)/$(LIB_DIR)/$(GL_LIB_NAME) ]; then \
		$(MINSTALL) $(TOP)/$(LIB_DIR)/libGL* $(INSTALL_DIR)/$(LIB_DIR); \
	fi

# Emacs tags
tags:
	etags `find . -name \*.[ch]` $(TOP)/include/GL/*.h

clean:
	-rm -f *.o


include depend
