#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/u_format.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"

#include "psl1ght_winsys.h"

#include "nouveau/nouveau_winsys.h"
#include "nouveau/nouveau_screen.h"

static void
psl1ght_destroy_winsys(struct pipe_winsys *s)
{
	struct psl1ght_winsys *ps_winsys = psl1ght_winsys(s);
	FREE(ps_winsys);
}

struct pipe_screen *
psl1ght_screen_create()
{
	struct psl1ght_winsys *psws;
	struct pipe_winsys *ws;
	static struct nouveau_device dev = { .chipset = 0x47 };

	psws = CALLOC_STRUCT(psl1ght_winsys);
	if (!psws) {
		return NULL;
	}
	ws = &psws->base;
	ws->destroy = psl1ght_destroy_winsys;

	psws->pscreen = nvfx_screen_create(ws, &dev);
	if (!psws->pscreen) {
		ws->destroy(ws);
		return NULL;
	}

	return psws->pscreen;
}
