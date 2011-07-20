#include <stdio.h>

#include "pipe/p_compiler.h"
#include "util/u_format.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "state_tracker/sw_winsys.h"

#include "psl1ght_sw_winsys.h"

struct psl1ght_sw_winsys
{
   struct sw_winsys base;

   enum pipe_format format;
};

static INLINE struct psl1ght_sw_winsys *
psl1ght_sw_winsys(struct sw_winsys *ws)
{
   return (struct psl1ght_sw_winsys *) ws;
}


static void
psl1ght_displaytarget_display(struct sw_winsys *ws,
			      struct sw_displaytarget *dt,
			      void *context_private)
{
   printf("psl1ght_displaytarget_display not implemented yet\n");
}

static void
psl1ght_displaytarget_unmap(struct sw_winsys *ws,
			    struct sw_displaytarget *dt)
{
   printf("psl1ght_displaytarget_unmap not implemented yet\n");
}

static void *
psl1ght_displaytarget_map(struct sw_winsys *ws,
			  struct sw_displaytarget *dt,
			  unsigned flags)
{
   printf("psl1ght_displaytarget_unmap not implemented yet\n");
}

static void
psl1ght_displaytarget_destroy(struct sw_winsys *ws,
			      struct sw_displaytarget *dt)
{
   printf("psl1ght_displaytarget_destroy not implemented yet\n");
}

static struct sw_displaytarget *
psl1ght_displaytarget_create(struct sw_winsys *ws,
			     unsigned tex_usage,
			     enum pipe_format format,
			     unsigned width, unsigned height,
			     unsigned alignment,
			     unsigned *stride)
{
   printf("psl1ght_displaytarget_create not implemented yet\n");
   return NULL;
}

static boolean
psl1ght_is_displaytarget_format_supported(struct sw_winsys *ws,
					  unsigned tex_usage,
					  enum pipe_format format)
{
   struct psl1ght_sw_winsys *psl1ght = psl1ght_sw_winsys(ws);
   return (psl1ght->format == format);
}

static void
psl1ght_destroy(struct sw_winsys *ws)
{
   struct psl1ght_sw_winsys *psl1ght = psl1ght_sw_winsys(ws);

   FREE(psl1ght);
}


struct sw_winsys *
psl1ght_create_sw_winsys(enum pipe_format format)
{
   struct psl1ght_sw_winsys *psl1ght;

   psl1ght = CALLOC_STRUCT(psl1ght_sw_winsys);
   if (!psl1ght)
      return NULL;

   psl1ght->format = format;

   psl1ght->base.destroy = psl1ght_destroy;
   psl1ght->base.is_displaytarget_format_supported =
      psl1ght_is_displaytarget_format_supported;

   psl1ght->base.displaytarget_create = psl1ght_displaytarget_create;
   psl1ght->base.displaytarget_destroy = psl1ght_displaytarget_destroy;
   psl1ght->base.displaytarget_map = psl1ght_displaytarget_map;
   psl1ght->base.displaytarget_unmap = psl1ght_displaytarget_unmap;

   psl1ght->base.displaytarget_display = psl1ght_displaytarget_display;

   return &psl1ght->base;
}
