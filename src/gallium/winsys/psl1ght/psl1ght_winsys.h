#ifndef __PSL1GHT_WINSYS_H__
#define __PSL1GHT_WINSYS_H__

#include "util/u_simple_screen.h"

struct psl1ght_winsys {
	struct pipe_winsys base;

	struct pipe_screen *pscreen;
};

static INLINE struct psl1ght_winsys *
psl1ght_winsys(struct pipe_winsys *ws)
{
	return (struct psl1ght_winsys *)ws;
}

static INLINE struct psl1ght_winsys *
psl1ght_winsys_screen(struct pipe_screen *pscreen)
{
	return psl1ght_winsys(pscreen->winsys);
}

#endif
