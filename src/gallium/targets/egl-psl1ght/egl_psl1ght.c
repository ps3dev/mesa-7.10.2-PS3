#include "common/egl_g3d_loader.h"
#include "state_tracker/st_gl_api.h"
#include "vg_api.h"
#include "target-helpers/inline_sw_helper.h"
#include "target-helpers/inline_debug_helper.h"
#include "egldriver.h"

static struct st_api *stapis[ST_API_COUNT];

static struct st_api *
get_st_api(enum st_api_type api)
{
   struct st_api *stapi;

   stapi = stapis[api];
   if (stapi)
      return stapi;

   switch (api) {
#if FEATURE_GL || FEATURE_ES1 || FEATURE_ES2
   case ST_API_OPENGL:
      stapi = st_gl_api_create();
      break;
#endif
#if FEATURE_VG
   case ST_API_OPENVG:
      stapi = (struct st_api *) vg_api_get();
      break;
#endif
   default:
      break;
   }

   stapis[api] = stapi;

   return stapi;
}

static struct st_api *
guess_gl_api(enum st_profile_type profile)
{
   return get_st_api(ST_API_OPENGL);
}

static struct pipe_screen *
create_drm_screen(const char *name, int fd)
{
   return NULL;
}

static struct pipe_screen *
create_sw_screen(struct sw_winsys *ws)
{
   struct pipe_screen *screen;

   screen = sw_screen_create(ws);
   if (screen)
      screen = debug_screen_wrap(screen);

   return screen;
}

static void
init_loader(struct egl_g3d_loader *loader)
{
#if FEATURE_GL
   loader->profile_masks[ST_API_OPENGL] |= ST_PROFILE_DEFAULT_MASK;
#endif
#if FEATURE_ES1
   loader->profile_masks[ST_API_OPENGL] |= ST_PROFILE_OPENGL_ES1_MASK;
#endif
#if FEATURE_ES2
   loader->profile_masks[ST_API_OPENGL] |= ST_PROFILE_OPENGL_ES2_MASK;
#endif
#if FEATURE_VG
   loader->profile_masks[ST_API_OPENVG] |= ST_PROFILE_DEFAULT_MASK;
#endif

   loader->get_st_api = get_st_api;
   loader->guess_gl_api = guess_gl_api;
   loader->create_drm_screen = create_drm_screen;
   loader->create_sw_screen = create_sw_screen;
}

static void
egl_g3d_unload(_EGLDriver *drv)
{
   int i;

   egl_g3d_destroy_driver(drv);

   for (i = 0; i < ST_API_COUNT; i++) {
      if (stapis[i]) {
         stapis[i]->destroy(stapis[i]);
         stapis[i] = NULL;
      }
   }
}

static struct egl_g3d_loader loader;

_EGLDriver *
psl1ght_eglMain(const char *args)
{
   _EGLDriver *drv;

   init_loader(&loader);
   drv = egl_g3d_create_driver(&loader);
   if (drv) {
      drv->Name = "Gallium";
      drv->Unload = egl_g3d_unload;
   }

   return drv;
}
