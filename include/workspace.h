#ifndef XDPW_WORKSPACE_H
#define XDPW_WORKSPACE_H

#include "screencast_common.h"

void xdpw_workspace_bind(struct xdpw_screencast_context *ctx, struct wl_registry *registry,
	uint32_t id);
void xdpw_workspace_finish(struct xdpw_screencast_context *ctx);

struct xdpw_wlr_output *xdpw_workspace_get_output(struct xdpw_workspace *workspace);
void xdpw_workspace_output_remove(struct xdpw_screencast_context *ctx, struct wl_output *output);

#endif
