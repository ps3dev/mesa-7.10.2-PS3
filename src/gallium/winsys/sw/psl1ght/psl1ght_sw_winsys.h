#ifndef PSL1GHT_SW_WINSYS
#define PSL1GHT_SW_WINSYS

struct sw_winsys;
enum pipe_format;

struct sw_winsys *
psl1ght_create_sw_winsys(enum pipe_format format);

#endif /* FBDEV_SW_WINSYS */
