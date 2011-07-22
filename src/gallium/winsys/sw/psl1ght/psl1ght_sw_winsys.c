#include "pipe/p_compiler.h"
#include "util/u_format.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "state_tracker/sw_winsys.h"

#include <malloc.h>
#include <rsx/rsx.h>

#include "psl1ght_sw_winsys.h"

#define CB_SIZE         0x10000
#define HOST_SIZE       (1024*1024)

#define NO_BUFFER       ((u32)~0)
#define MAX_BUFFERS     8

struct psl1ght_sw_displaytarget
{
   unsigned width;
   unsigned height;
   unsigned stride;

   void *data;
   u32 offset;
   u32 bufferId;
};

struct psl1ght_sw_winsys
{
   struct sw_winsys base;

   gcmContextData *ctx;
   u32 buffersUsed;
};

static INLINE struct psl1ght_sw_displaytarget *
psl1ght_sw_displaytarget(struct sw_displaytarget *dt)
{
   return (struct psl1ght_sw_displaytarget *) dt;
}

static INLINE struct psl1ght_sw_winsys *
psl1ght_sw_winsys(struct sw_winsys *ws)
{
   return (struct psl1ght_sw_winsys *) ws;
}


static boolean
psl1ght_displaytarget_set_buffer(struct psl1ght_sw_winsys *psl1ght,
				 struct psl1ght_sw_displaytarget *psdt)
{
   u32 bid;

   if (psdt->bufferId != NO_BUFFER)
      return TRUE;
   for (bid = 0; bid < MAX_BUFFERS; bid++) {
      if (!(psl1ght->buffersUsed & (1 << bid))) {
	 if (gcmSetDisplayBuffer(bid, psdt->offset, psdt->stride,
				 psdt->width, psdt->height))
	     return FALSE;
	 psl1ght->buffersUsed |= (1 << bid);
	 psdt->bufferId = bid;
	 return TRUE;
      }
   }
   return FALSE;
}

static void
psl1ght_displaytarget_display(struct sw_winsys *ws,
			      struct sw_displaytarget *dt,
			      void *context_private)
{
   struct psl1ght_sw_winsys *psl1ght = psl1ght_sw_winsys(ws);
   struct psl1ght_sw_displaytarget *psdt = psl1ght_sw_displaytarget(dt);

   if (psdt->bufferId == NO_BUFFER) {
       if (!psl1ght_displaytarget_set_buffer(psl1ght, psdt))
	  return;
   }

   gcmResetFlipStatus();
   if (!gcmSetFlip(psl1ght->ctx, psdt->bufferId)) {
      rsxFlushBuffer(psl1ght->ctx);
      gcmSetWaitFlip(psl1ght->ctx);
   }
}

static void
psl1ght_displaytarget_unmap(struct sw_winsys *ws,
			    struct sw_displaytarget *dt)
{
   /* no-op */
}

static void *
psl1ght_displaytarget_map(struct sw_winsys *ws,
			  struct sw_displaytarget *dt,
			  unsigned flags)
{
   struct psl1ght_sw_displaytarget *psdt = psl1ght_sw_displaytarget(dt);
   return psdt->data;
}

static void
psl1ght_displaytarget_destroy(struct sw_winsys *ws,
			      struct sw_displaytarget *dt)
{
   struct psl1ght_sw_winsys *psl1ght = psl1ght_sw_winsys(ws);
   struct psl1ght_sw_displaytarget *psdt = psl1ght_sw_displaytarget(dt);

   if (psdt->bufferId != NO_BUFFER)
      psl1ght->buffersUsed &= ~(1 << psdt->bufferId);

   if (psdt->data)
      rsxFree(psdt->data);

   FREE(psdt);
}

static struct sw_displaytarget *
psl1ght_displaytarget_create(struct sw_winsys *ws,
			     unsigned tex_usage,
			     enum pipe_format format,
			     unsigned width, unsigned height,
			     unsigned alignment,
			     unsigned *stride)
{
   struct psl1ght_sw_winsys *psl1ght = psl1ght_sw_winsys(ws);
   struct psl1ght_sw_displaytarget *psdt;

   psdt = CALLOC_STRUCT(psl1ght_sw_displaytarget);
   if (!psdt)
      return NULL;

   psdt->width = width;
   psdt->height = height;
   psdt->stride = width * 4;
   psdt->bufferId = NO_BUFFER;

   psdt->data = rsxMemalign(64, psdt->height * psdt->stride);
   if (!psdt->data) {
      FREE(psdt);
      return NULL;
   }
   if (rsxAddressToOffset(psdt->data, &psdt->offset)) {
      rsxFree(psdt->data);
      FREE(psdt);
      return NULL;
   }

   *stride = psdt->stride;

   return (struct sw_displaytarget *) psdt;
}

static boolean
psl1ght_is_displaytarget_format_supported(struct sw_winsys *ws,
					  unsigned tex_usage,
					  enum pipe_format format)
{
   struct psl1ght_sw_winsys *psl1ght = psl1ght_sw_winsys(ws);
   return (PIPE_FORMAT_X8R8G8B8_UNORM == format ||
	   PIPE_FORMAT_X8B8G8R8_UNORM == format
	   /* FIXME: || float format? */);
}

static void
psl1ght_destroy(struct sw_winsys *ws)
{
   struct psl1ght_sw_winsys *psl1ght = psl1ght_sw_winsys(ws);

   FREE(psl1ght);
}


struct sw_winsys *
psl1ght_create_sw_winsys(gcmContextData *ctx)
{
   struct psl1ght_sw_winsys *psl1ght;

   if (ctx == NULL) {
      void *host_addr = memalign (1024*1024, HOST_SIZE);
      if (host_addr == NULL)
	 return NULL;
      ctx = rsxInit(CB_SIZE, HOST_SIZE, host_addr);
      if (ctx == NULL)
	 return NULL;
   }

   if (!rsxHeapInit())
      return NULL;

   gcmSetFlipMode (GCM_FLIP_VSYNC); // Wait for VSYNC to flip

   psl1ght = CALLOC_STRUCT(psl1ght_sw_winsys);
   if (!psl1ght)
      return NULL;

   psl1ght->ctx = ctx;

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
