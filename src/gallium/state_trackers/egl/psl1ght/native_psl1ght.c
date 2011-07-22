#include "pipe/p_screen.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_pointer.h"

#include "common/native.h"
#include "common/native_helper.h"
#include "egldefines.h"

#include <stdio.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>

#include "psl1ght/psl1ght_sw_winsys.h"

struct psl1ght_mode {
     struct native_mode base;

     u16 refreshRate;
     const videoDisplayMode *vmode;
     struct psl1ght_mode *prev;
     char desc[1];
};

struct psl1ght_connector {
    struct native_connector base;

    u32 videoOut;

    videoDeviceInfo deviceInfo;
    int numModes;
    struct psl1ght_mode *lastMode;
};

struct psl1ght_display {
    struct native_display base;

    struct native_event_handler *event_handler;

    struct psl1ght_connector primary_connector;
    struct psl1ght_connector secondary_connector;

    struct psl1ght_surface *current_surface;
};

struct psl1ght_surface {
   struct native_surface base;

   struct psl1ght_display *psdpy;
   struct resource_surface *rsurf;
   int width, height;
   u8 format;

   unsigned int sequence_number;

   boolean is_current;
};

static INLINE struct psl1ght_mode *
psl1ght_mode(const struct native_mode *nmode)
{
   return (struct psl1ght_mode *) nmode;
}

static INLINE struct psl1ght_connector *
psl1ght_connector(const struct native_connector *nconn)
{
   return (struct psl1ght_connector *) nconn;
}

static INLINE struct psl1ght_display *
psl1ght_display(const struct native_display *ndpy)
{
   return (struct psl1ght_display *) ndpy;
}

static INLINE struct psl1ght_surface *
psl1ght_surface(const struct native_surface *nsurf)
{
   return (struct psl1ght_surface *) nsurf;
}

static const struct native_config psl1ght_configs[] =
  {
    { .buffer_mask = (1 << NATIVE_ATTACHMENT_FRONT_LEFT) |
      (1 << NATIVE_ATTACHMENT_BACK_LEFT),
      .color_format = PIPE_FORMAT_X8R8G8B8_UNORM,
      .scanout_bit = TRUE,
      .native_visual_id = VIDEO_BUFFER_FORMAT_XRGB,
    },
    { .buffer_mask = (1 << NATIVE_ATTACHMENT_FRONT_LEFT) |
      (1 << NATIVE_ATTACHMENT_BACK_LEFT),
      .color_format = PIPE_FORMAT_X8B8G8R8_UNORM,
      .scanout_bit = TRUE,
      .native_visual_id = VIDEO_BUFFER_FORMAT_XBGR,
    },
    /* FIXME: What pipe format?
    { .buffer_mask = (1 << NATIVE_ATTACHMENT_FRONT_LEFT) |
      (1 << NATIVE_ATTACHMENT_BACK_LEFT),
      .color_format = ????,
      .scanout_bit = TRUE,
      .native_visual_id = VIDEO_BUFFER_FORMAT_FLOAT,
    },
    */
  };

static boolean
psl1ght_surface_validate(struct native_surface *nsurf, uint attachment_mask,
			 unsigned int *seq_num, struct pipe_resource **textures,
			 int *width, int *height)
{
   struct psl1ght_surface *pssurf = psl1ght_surface(nsurf);

   if (!resource_surface_add_resources(pssurf->rsurf, attachment_mask))
      return FALSE;
   if (textures)
      resource_surface_get_resources(pssurf->rsurf, textures, attachment_mask);

   if (seq_num)
      *seq_num = pssurf->sequence_number;
   if (width)
      *width = pssurf->width;
   if (height)
      *height = pssurf->height;

   return TRUE;
}

static boolean
psl1ght_surface_flush_frontbuffer(struct native_surface *nsurf)
{
   struct psl1ght_surface *pssurf = psl1ght_surface(nsurf);

   if (!pssurf->is_current)
      return TRUE;

   return resource_surface_present(pssurf->rsurf,
         NATIVE_ATTACHMENT_FRONT_LEFT, NULL);
}

static boolean
psl1ght_surface_swap_buffers(struct native_surface *nsurf)
{
   struct psl1ght_surface *pssurf = psl1ght_surface(nsurf);
   struct psl1ght_display *psdpy = pssurf->psdpy;
   boolean ret = TRUE;

   if (pssurf->is_current) {
      ret = resource_surface_present(pssurf->rsurf,
            NATIVE_ATTACHMENT_BACK_LEFT, NULL);
   }

   resource_surface_swap_buffers(pssurf->rsurf,
         NATIVE_ATTACHMENT_FRONT_LEFT, NATIVE_ATTACHMENT_BACK_LEFT, FALSE);
   /* the front/back textures are swapped */
   pssurf->sequence_number++;
   psdpy->event_handler->invalid_surface(&psdpy->base,
         &pssurf->base, pssurf->sequence_number);

   return ret;
}

static boolean
psl1ght_surface_present(struct native_surface *nsurf,
			enum native_attachment natt,
			boolean preserve,
			uint swap_interval)
{
   boolean ret;

   if (preserve || swap_interval)
      return FALSE;

   switch (natt) {
   case NATIVE_ATTACHMENT_FRONT_LEFT:
      ret = psl1ght_surface_flush_frontbuffer(nsurf);
      break;
   case NATIVE_ATTACHMENT_BACK_LEFT:
      ret = psl1ght_surface_swap_buffers(nsurf);
      break;
   default:
      ret = FALSE;
      break;
   }

   return ret;
}

static void
psl1ght_surface_wait(struct native_surface *nsurf)
{
   /* no-op */
}

static void
psl1ght_surface_destroy(struct native_surface *nsurf)
{
   struct psl1ght_surface *pssurf = psl1ght_surface(nsurf);

   resource_surface_destroy(pssurf->rsurf);
   FREE(pssurf);
}

static struct native_surface *
psl1ght_display_create_scanout_surface(struct native_display *ndpy,
				       const struct native_config *nconf,
				       uint width, uint height)
{
   struct psl1ght_display *psdpy = psl1ght_display(ndpy);
   struct psl1ght_surface *pssurf;

   pssurf = CALLOC_STRUCT(psl1ght_surface);
   if (!pssurf)
      return NULL;

   pssurf->psdpy = psdpy;
   pssurf->width = width;
   pssurf->height = height;
   pssurf->format = nconf->native_visual_id;

   pssurf->rsurf = resource_surface_create(psdpy->base.screen,
         nconf->color_format,
         PIPE_BIND_RENDER_TARGET |
         PIPE_BIND_DISPLAY_TARGET |
         PIPE_BIND_SCANOUT);
   if (!pssurf->rsurf) {
      FREE(pssurf);
      return NULL;
   }

   resource_surface_set_size(pssurf->rsurf, pssurf->width, pssurf->height);

   pssurf->base.destroy = psl1ght_surface_destroy;
   pssurf->base.present = psl1ght_surface_present;
   pssurf->base.validate = psl1ght_surface_validate;
   pssurf->base.wait = psl1ght_surface_wait;

   return &pssurf->base;
}

static boolean
psl1ght_display_program(struct native_display *ndpy, int crtc_idx,
			struct native_surface *nsurf, uint x, uint y,
			const struct native_connector **nconns, int num_nconns,
			const struct native_mode *nmode)
{
   struct psl1ght_display *psdpy = psl1ght_display(ndpy);
   struct psl1ght_surface *pssurf = psl1ght_surface(nsurf);
   struct psl1ght_mode *psmode = psl1ght_mode(nmode);

   if (x || y)
      return FALSE;

   if (pssurf) {
      const videoDisplayMode *vmode = psmode->vmode;
      videoConfiguration config;
      int i;

      if(!resource_surface_add_resources(pssurf->rsurf,
					 (1 << NATIVE_ATTACHMENT_FRONT_LEFT) |
					 (1 << NATIVE_ATTACHMENT_BACK_LEFT)))
	 return FALSE;

      memset(&config, 0, sizeof(config));
      config.resolution = vmode->resolution;
      config.format = pssurf->format;
      config.aspect = vmode->aspect;
      config.pitch  = pssurf->width * 4;
      /* FIXME: scan mode & refresh rate? */

      for(i=0; i<num_nconns; i++) {
	 u32 videoOut = psl1ght_connector(nconns[i])->videoOut;
	 if (videoConfigure(videoOut, &config, NULL, 1))
	    return FALSE;
      }
   }

   if (psdpy->current_surface) {
      if (psdpy->current_surface == pssurf)
         return TRUE;
      /*...*/
      psdpy->current_surface->is_current = FALSE;
      psdpy->current_surface = NULL;
   }

   if (pssurf) {
      /*...*/
      pssurf->is_current = TRUE;
   }
   psdpy->current_surface = pssurf;

   return TRUE;
}

static const struct native_mode **
psl1ght_display_get_modes(struct native_display *ndpy,
			  const struct native_connector *nconn,
			  int *num_modes)
{
   struct psl1ght_connector *psconn = psl1ght_connector(nconn);
   const struct native_mode **modes;

   modes = MALLOC(sizeof(*modes)*psconn->numModes);
   if (modes) {
      struct psl1ght_mode *psmode = psconn->lastMode;
      int i;
      for (i=psconn->numModes; --i>=0; ) {
	 modes[i] = &psmode->base;
	 psmode = psmode->prev;
      }
      if (num_modes)
         *num_modes = psconn->numModes;
   }

   return modes;
}

static const struct native_connector **
psl1ght_display_get_connectors(struct native_display *ndpy, int *num_connectors,
			       int *num_crtc)
{
   struct psl1ght_display *psdpy = psl1ght_display(ndpy);
   const struct native_connector **connectors;

   connectors = MALLOC(2*sizeof(*connectors));
   if (connectors) {
      connectors[0] = &psdpy->primary_connector.base;
      connectors[1] = &psdpy->secondary_connector.base;
      if (num_connectors)
         *num_connectors = 2;
   }

   return connectors;
}

static struct native_display_modeset psl1ght_display_modeset = {
   .get_connectors = psl1ght_display_get_connectors,
   .get_modes = psl1ght_display_get_modes,
   .create_scanout_surface = psl1ght_display_create_scanout_surface,
   .program = psl1ght_display_program
};

static const struct native_config **
psl1ght_display_get_configs(struct native_display *ndpy, int *num_configs)
{
   const struct native_config **configs;

   configs = MALLOC(ARRAY_SIZE(psl1ght_configs)*sizeof(*configs));
   if (configs) {
      int i;
      for (i=0; i<ARRAY_SIZE(psl1ght_configs); i++)
	 configs[i] = &psl1ght_configs[i];
      if (num_configs)
         *num_configs = ARRAY_SIZE(psl1ght_configs);
   }

   return configs;
}

static int
psl1ght_display_get_param(struct native_display *ndpy,
			  enum native_param_type param)
{
   int val;

   switch (param) {
   case NATIVE_PARAM_USE_NATIVE_BUFFER:
   case NATIVE_PARAM_PRESERVE_BUFFER:
   case NATIVE_PARAM_MAX_SWAP_INTERVAL:
   default:
      val = 0;
      break;
   }

   return val;
}

static void
psl1ght_free_modes(struct native_connector *nconn)
{
   struct psl1ght_connector *psconn = psl1ght_connector(nconn);
   struct psl1ght_mode *m;
   while ((m = psconn->lastMode)) {
      psconn->lastMode = m->prev;
      free(m);
   }
   psconn->numModes = 0;
}

static void
psl1ght_display_destroy(struct native_display *ndpy)
{
   struct psl1ght_display *psdpy = psl1ght_display(ndpy);

   if (psdpy->base.screen)
      psdpy->base.screen->destroy(psdpy->base.screen);
   psl1ght_free_modes(&psdpy->primary_connector.base);
   psl1ght_free_modes(&psdpy->secondary_connector.base);
   FREE(psdpy);
}

static boolean
psl1ght_display_add_mode(struct native_connector *nconn,
			 const videoDisplayMode *vmode,
			 const videoResolution *vres,
			 u16 refresh1, int refresh2, const char *refresh3)
{
   char buf[32];
   struct psl1ght_mode *psmode;
   struct psl1ght_connector *psconn = psl1ght_connector(nconn);
   sprintf(buf, "%dx%d%c%s %sHz", vres->width, vres->height,
	   (vmode->scanMode == VIDEO_SCANMODE_INTERLACE? 'i':
	    (vmode->scanMode == VIDEO_SCANMODE_PROGRESSIVE? 'p':'*')),
	   (vmode->aspect == VIDEO_ASPECT_4_3? " (4:3)" :
	    (vmode->aspect == VIDEO_ASPECT_16_9? " (16:9)" : "")),
	   refresh3);
   psmode = MALLOC(sizeof(*psmode)+strlen(buf));
   if (!psmode)
      return FALSE;
   strcpy(psmode->desc, buf);
   psmode->base.desc = psmode->desc;
   psmode->base.width = vres->width;
   psmode->base.height = vres->height;
   psmode->base.refresh_rate = refresh2;
   psmode->refreshRate = refresh1;
   psmode->vmode = vmode;
   psmode->prev = psconn->lastMode;
   psconn->lastMode = psmode;
   psconn->numModes++;
   return TRUE;
}

static boolean
psl1ght_display_add_modes(struct native_connector *nconn,
			  const videoDisplayMode *vmode)
{
   static const struct { u16 from; u16 to; const char *name; } rates[] = {
      { VIDEO_REFRESH_59_94HZ, 59940, "59.94" },
      { VIDEO_REFRESH_50HZ,    50000, "50", },
      { VIDEO_REFRESH_60HZ,    60000, "60" },
      { VIDEO_REFRESH_30HZ,    30000, "30" },
   };
   videoResolution vres;
   int i;
   if (videoGetResolution(vmode->resolution, &vres))
      return FALSE;
   for (i=0; i<ARRAY_SIZE(rates); i++)
      if (vmode->refreshRates & rates[i].from)
	 if (!psl1ght_display_add_mode(nconn, vmode, &vres, rates[i].from, 
				       rates[i].to, rates[i].name))
	    return FALSE;
   return TRUE;
}

static boolean
psl1ght_display_init_connector(struct native_connector *nconn, u32 videoOut)
{
    int i;
    struct psl1ght_connector *psconn = psl1ght_connector(nconn);
    psconn->videoOut = videoOut;
    psconn->numModes = 0;
    psconn->lastMode = NULL;
    if(videoGetDeviceInfo(videoOut, 0, &psconn->deviceInfo))
       return FALSE;
    for(i=0; i<psconn->deviceInfo.availableModeCount; i++)
       if (!psl1ght_display_add_modes(nconn, &psconn->deviceInfo.availableModes[i]))
	  return FALSE;
    
    return TRUE;
}

static boolean
psl1ght_display_init(struct native_display *ndpy, gcmContextData *ctx)
{
   struct psl1ght_display *psdpy = psl1ght_display(ndpy);
   struct sw_winsys *ws;

   if (!psl1ght_display_init_connector(&psdpy->primary_connector.base,
				       VIDEO_PRIMARY) ||
       !psl1ght_display_init_connector(&psdpy->secondary_connector.base,
				       VIDEO_SECONDARY))
      return FALSE;

   ws = psl1ght_create_sw_winsys(ctx);
   if (ws) {
      psdpy->base.screen =
         psdpy->event_handler->new_sw_screen(&psdpy->base, ws);
   }

   return (psdpy->base.screen != NULL);
}

static struct native_display *
psl1ght_display_create(gcmContextData *ctx,
		       struct native_event_handler *event_handler,
		       void *user_data)
{
   struct psl1ght_display *psdpy;

   psdpy = CALLOC_STRUCT(psl1ght_display);
   if (!psdpy)
      return NULL;

   psdpy->event_handler = event_handler;
   psdpy->base.user_data = user_data;

   if (!psl1ght_display_init(&psdpy->base, ctx)) {
      psl1ght_display_destroy(&psdpy->base);
      return NULL;
   }

   psdpy->base.destroy = psl1ght_display_destroy;
   psdpy->base.get_param = psl1ght_display_get_param;
   psdpy->base.get_configs = psl1ght_display_get_configs;

   psdpy->base.modeset = &psl1ght_display_modeset;

   return &psdpy->base;
}

static struct native_display *
native_create_display(void *dpy, struct native_event_handler *event_handler,
                      void *user_data)
{
   struct native_display *ndpy;
   gcmContextData *ctx = (gcmContextData *)dpy;

   ndpy = psl1ght_display_create(ctx, event_handler, user_data);

   return ndpy;
}

static const struct native_platform psl1ght_platform = {
   "PSL1GHT", /* name */
   native_create_display
};

const struct native_platform *
native_get_psl1ght_platform(void)
{
   return &psl1ght_platform;
}
