TOP = ../../../..
include $(TOP)/configs/current

LIBNAME = eglpsl1ght

LIBRARY_INCLUDES = \
	-I$(TOP)/src/gallium/state_trackers/egl \
	-I$(TOP)/src/gallium/state_trackers/vega \
	-I$(TOP)/src/egl/main \
	-I$(TOP)/src/mesa

LIBRARY_DEFINES = -DGALLIUM_SOFTPIPE

ifneq ($(filter $(GL_LIB), $(EGL_CLIENT_APIS)),)
LIBRARY_DEFINES += -DFEATURE_GL=1
endif
ifneq ($(filter $(GLESv1_CM_LIB), $(EGL_CLIENT_APIS)),)
LIBRARY_DEFINES += -DFEATURE_ES1=1
endif
ifneq ($(filter $(GLESv2_LIB), $(EGL_CLIENT_APIS)),)
LIBRARY_DEFINES += -DFEATURE_ES2=1
endif
ifneq ($(filter $(VG_LIB), $(EGL_CLIENT_APIS)),)
LIBRARY_DEFINES += -DFEATURE_VG=1
endif

C_SOURCES = egl_psl1ght.c

include ../../Makefile.template
