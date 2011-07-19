#include "pipe/p_screen.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_pointer.h"

#include "common/native.h"
#include "common/native_helper.h"

static struct native_display *
native_create_display(void *dpy, struct native_event_handler *event_handler,
                      void *user_data)
{
   struct native_display *ndpy;

   /* FIXME */
   return NULL;
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
