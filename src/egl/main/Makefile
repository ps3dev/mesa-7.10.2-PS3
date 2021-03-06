# src/egl/main/Makefile

TOP = ../../..
include $(TOP)/configs/current


EGL_MAJOR = 1
EGL_MINOR = 0

INCLUDE_DIRS = -I$(TOP)/include

HEADERS = \
	eglcompiler.h \
	eglconfig.h \
	eglcontext.h \
	eglcurrent.h \
	egldefines.h \
	egldisplay.h \
	egldriver.h \
	eglglobals.h \
	eglimage.h \
	egllog.h \
	eglmisc.h \
	eglmode.h \
	eglmutex.h \
	eglscreen.h \
	eglstring.h \
	eglsurface.h \
	eglsync.h

SOURCES = \
	eglapi.c \
	eglarray.c \
	eglconfig.c \
	eglcontext.c \
	eglcurrent.c \
	egldisplay.c \
	egldriver.c \
	eglfallbacks.c \
	eglglobals.c \
	eglimage.c \
	egllog.c \
	eglmisc.c \
	eglmode.c \
	eglscreen.c \
	eglstring.c \
	eglsurface.c \
	eglsync.c

OBJECTS = $(SOURCES:.c=.o)


# use dl*() to load drivers
LOCAL_CFLAGS = -D_EGL_OS_UNIX=1

# translate --with-egl-platforms to _EGLPlatformType
EGL_NATIVE_PLATFORM=_EGL_INVALID_PLATFORM
ifeq ($(firstword $(EGL_PLATFORMS)),x11)
EGL_NATIVE_PLATFORM=_EGL_PLATFORM_X11
endif
ifeq ($(firstword $(EGL_PLATFORMS)),drm)
EGL_NATIVE_PLATFORM=_EGL_PLATFORM_DRM
endif
ifeq ($(firstword $(EGL_PLATFORMS)),fbdev)
EGL_NATIVE_PLATFORM=_EGL_PLATFORM_FBDEV
endif
ifeq ($(firstword $(EGL_PLATFORMS)),psl1ght)
EGL_NATIVE_PLATFORM=_EGL_PLATFORM_PSL1GHT
LOCAL_CFLAGS := -D_EGL_OS_PSL1GHT=1
endif

LOCAL_CFLAGS += \
	-D_EGL_NATIVE_PLATFORM=$(EGL_NATIVE_PLATFORM) \
	-D_EGL_DRIVER_SEARCH_DIR=\"$(EGL_DRIVER_INSTALL_DIR)\"

.c.o:
	$(CC) -c $(INCLUDE_DIRS) $(CFLAGS) $(LOCAL_CFLAGS) $< -o $@



default: depend library


# EGL Library
library: $(TOP)/$(LIB_DIR)/$(EGL_LIB_NAME)

$(TOP)/$(LIB_DIR)/$(EGL_LIB_NAME): $(OBJECTS)
	$(MKLIB) -o $(EGL_LIB) -linker '$(CC)' -ldflags '$(LDFLAGS)' \
		-major $(EGL_MAJOR) -minor $(EGL_MINOR) \
		-install $(TOP)/$(LIB_DIR) $(MKLIB_OPTIONS) \
		$(EGL_LIB_DEPS) $(OBJECTS)

install-headers:
	$(INSTALL) -d $(DESTDIR)$(INSTALL_INC_DIR)/KHR
	$(INSTALL) -m 644 $(TOP)/include/KHR/*.h \
		$(DESTDIR)$(INSTALL_INC_DIR)/KHR
	$(INSTALL) -d $(DESTDIR)$(INSTALL_INC_DIR)/EGL
	$(INSTALL) -m 644 $(TOP)/include/EGL/*.h \
		$(DESTDIR)$(INSTALL_INC_DIR)/EGL

PKG_CONFIG_DIR = $(INSTALL_LIB_DIR)/pkgconfig

gl_pcedit = sed \
	-e 's,@INSTALL_DIR@,$(INSTALL_DIR),' \
	-e 's,@INSTALL_LIB_DIR@,$(INSTALL_LIB_DIR),' \
	-e 's,@INSTALL_INC_DIR@,$(INSTALL_INC_DIR),' \
	-e 's,@VERSION@,$(MESA_MAJOR).$(MESA_MINOR).$(MESA_TINY),' \
	-e 's,@EGL_PC_REQ_PRIV@,$(EGL_PC_REQ_PRIV),' \
	-e 's,@EGL_PC_LIB_PRIV@,$(EGL_PC_LIB_PRIV),' \
	-e 's,@EGL_PC_CFLAGS@,$(EGL_PC_CFLAGS),' \
	-e 's,@EGL_LIB@,$(EGL_LIB),'

egl.pc: egl.pc.in
	$(gl_pcedit) $< > $@

install: default install-headers egl.pc
	$(INSTALL) -d $(DESTDIR)$(INSTALL_LIB_DIR)
	$(MINSTALL) $(TOP)/$(LIB_DIR)/$(EGL_LIB_GLOB) \
		$(DESTDIR)$(INSTALL_LIB_DIR)
	$(INSTALL) -d $(DESTDIR)$(PKG_CONFIG_DIR)
	$(INSTALL) -m 644 egl.pc $(DESTDIR)$(PKG_CONFIG_DIR)

clean:
	-rm -f *.o
	-rm -f depend depend.bak


depend: $(SOURCES) $(HEADERS)
	@ echo "running $(MKDEP)"
	@ rm -f depend
	@ touch depend
	$(MKDEP) $(MKDEP_OPTIONS) $(DEFINES) $(INCLUDE_DIRS) \
		$(SOURCES) $(HEADERS) > /dev/null 2>/dev/null


-include depend
# DO NOT DELETE
