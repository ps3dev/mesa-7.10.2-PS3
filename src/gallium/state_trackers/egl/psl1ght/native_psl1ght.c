#include "pipe/p_screen.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_pointer.h"

#include "common/native.h"
#include "common/native_helper.h"
#include "psl1ght/psl1ght_sw_winsys.h"
#include "egldefines.h"

#include <stdio.h>
#include <sysutil/video.h>

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
};

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

static struct native_surface *
psl1ght_display_create_scanout_surface(struct native_display *ndpy,
				       const struct native_config *nconf,
				       uint width, uint height)
{
   printf("psl1ght_display_create_scanout_surface not implemented yet\n");
   return NULL;
}

static boolean
psl1ght_display_program(struct native_display *ndpy, int crtc_idx,
			struct native_surface *nsurf, uint x, uint y,
			const struct native_connector **nconns, int num_nconns,
			const struct native_mode *nmode)
{
   printf("psl1ght_display_program not implemented yet\n");
   return FALSE;
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
psl1ght_display_init(struct native_display *ndpy)
{
   struct psl1ght_display *psdpy = psl1ght_display(ndpy);
   struct sw_winsys *ws;

   if (!psl1ght_display_init_connector(&psdpy->primary_connector.base,
				       VIDEO_PRIMARY) ||
       !psl1ght_display_init_connector(&psdpy->secondary_connector.base,
				       VIDEO_SECONDARY))
      return FALSE;

   ws = psl1ght_create_sw_winsys(PIPE_FORMAT_X8R8G8B8_UNORM);
   if (ws) {
      psdpy->base.screen =
         psdpy->event_handler->new_sw_screen(&psdpy->base, ws);
   }

   if (psdpy->base.screen) {
      if (!psdpy->base.screen->is_format_supported(psdpy->base.screen,
               PIPE_FORMAT_X8R8G8B8_UNORM, PIPE_TEXTURE_2D, 0,
               PIPE_BIND_RENDER_TARGET, 0)) {
         psdpy->base.screen->destroy(psdpy->base.screen);
         psdpy->base.screen = NULL;
      }
   }

   return (psdpy->base.screen != NULL);
}

static struct native_display *
psl1ght_display_create(void *dpy, struct native_event_handler *event_handler,
		       void *user_data)
{
   struct psl1ght_display *psdpy;

   psdpy = CALLOC_STRUCT(psl1ght_display);
   if (!psdpy)
      return NULL;

   psdpy->event_handler = event_handler;
   psdpy->base.user_data = user_data;

   if (!psl1ght_display_init(&psdpy->base)) {
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

   ndpy = psl1ght_display_create(dpy, event_handler, user_data);

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
