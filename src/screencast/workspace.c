#include <stdlib.h>
#include <string.h>

#include "ext-workspace-v1-client-protocol.h"
#include "screencast.h"
#include "workspace.h"

static void workspace_destroy(struct xdpw_workspace *workspace) {
	struct xdpw_screencast_instance *cast, *tmp;
	wl_list_for_each_safe(cast, tmp, &workspace->ctx->screencast_instances, link) {
		if (cast->target->workspace == workspace) {
			xdpw_screencast_instance_destroy(cast);
		}
	}
	wl_list_remove(&workspace->link);
	ext_workspace_handle_v1_destroy(workspace->handle);
	free(workspace->name);
	free(workspace);
}

static void workspace_id(void *data, struct ext_workspace_handle_v1 *handle, const char *id) {
}

static void workspace_coordinates(void *data, struct ext_workspace_handle_v1 *handle,
		struct wl_array *coordinates) {
}

static void workspace_state(void *data, struct ext_workspace_handle_v1 *handle, uint32_t state) {
}

static void workspace_capabilities(void *data, struct ext_workspace_handle_v1 *handle,
		uint32_t capabilities) {
}

static void workspace_name(void *data, struct ext_workspace_handle_v1 *handle, const char *name) {
	struct xdpw_workspace *workspace = data;
	free(workspace->name);
	workspace->name = strdup(name);
}

static void workspace_removed(void *data, struct ext_workspace_handle_v1 *handle) {
	workspace_destroy(data);
}

static const struct ext_workspace_handle_v1_listener workspace_listener = {
	.id = workspace_id,
	.name = workspace_name,
	.coordinates = workspace_coordinates,
	.state = workspace_state,
	.capabilities = workspace_capabilities,
	.removed = workspace_removed,
};

static void manager_workspace(void *data, struct ext_workspace_manager_v1 *manager,
		struct ext_workspace_handle_v1 *handle) {
	struct xdpw_screencast_context *ctx = data;
	struct xdpw_workspace *workspace = calloc(1, sizeof(*workspace));
	if (!workspace) {
		ext_workspace_handle_v1_destroy(handle);
		return;
	}
	workspace->ctx = ctx;
	workspace->handle = handle;
	wl_list_insert(&ctx->workspaces, &workspace->link);
	ext_workspace_handle_v1_add_listener(handle, &workspace_listener, workspace);
}

struct workspace_output {
	struct wl_list link;
	struct wl_output *output;
};

static void group_output_enter(void *data, struct ext_workspace_group_handle_v1 *handle,
		struct wl_output *output) {
	struct xdpw_workspace_group *group = data;
	struct workspace_output *entry;
	wl_list_for_each(entry, &group->outputs, link) {
		if (entry->output == output) {
			return;
		}
	}
	entry = calloc(1, sizeof(*entry));
	if (entry) {
		entry->output = output;
		wl_list_insert(group->outputs.prev, &entry->link);
	}
}

static void group_output_leave(void *data, struct ext_workspace_group_handle_v1 *handle,
		struct wl_output *output) {
	struct xdpw_workspace_group *group = data;
	struct workspace_output *entry, *tmp;
	wl_list_for_each_safe(entry, tmp, &group->outputs, link) {
		if (entry->output == output) {
			wl_list_remove(&entry->link);
			free(entry);
		}
	}
}

void xdpw_workspace_output_remove(struct xdpw_screencast_context *ctx, struct wl_output *output) {
	struct xdpw_workspace_group *group;
	wl_list_for_each(group, &ctx->workspace_groups, link) {
		group_output_leave(group, group->handle, output);
	}
}

struct xdpw_wlr_output *xdpw_workspace_get_output(struct xdpw_workspace *workspace) {
	if (!workspace->group) {
		return NULL;
	}
	struct workspace_output *entry;
	wl_list_for_each(entry, &workspace->group->outputs, link) {
		struct xdpw_wlr_output *output;
		wl_list_for_each(output, &workspace->ctx->output_list, link) {
			if (output->output == entry->output) {
				return output;
			}
		}
	}
	return NULL;
}

static void group_workspace_enter(void *data, struct ext_workspace_group_handle_v1 *handle,
		struct ext_workspace_handle_v1 *handle_workspace) {
	struct xdpw_workspace *workspace = ext_workspace_handle_v1_get_user_data(handle_workspace);
	if (workspace) {
		workspace->group = data;
	}
}

static void group_workspace_leave(void *data, struct ext_workspace_group_handle_v1 *handle,
		struct ext_workspace_handle_v1 *handle_workspace) {
	struct xdpw_workspace *workspace = ext_workspace_handle_v1_get_user_data(handle_workspace);
	if (workspace && workspace->group == data) {
		workspace->group = NULL;
	}
}

static void group_removed(void *data, struct ext_workspace_group_handle_v1 *handle) {
	struct xdpw_workspace_group *group = data;
	struct xdpw_workspace *workspace;
	wl_list_for_each(workspace, &group->ctx->workspaces, link) {
		if (workspace->group == group) {
			workspace->group = NULL;
		}
	}
	struct workspace_output *entry, *tmp;
	wl_list_for_each_safe(entry, tmp, &group->outputs, link) {
		wl_list_remove(&entry->link);
		free(entry);
	}
	wl_list_remove(&group->link);
	ext_workspace_group_handle_v1_destroy(handle);
	free(group);
}

static void group_capabilities(void *data, struct ext_workspace_group_handle_v1 *handle,
		uint32_t capabilities) {
}

static const struct ext_workspace_group_handle_v1_listener group_listener = {
	.capabilities = group_capabilities,
	.output_enter = group_output_enter,
	.output_leave = group_output_leave,
	.workspace_enter = group_workspace_enter,
	.workspace_leave = group_workspace_leave,
	.removed = group_removed,
};

static void manager_group(void *data, struct ext_workspace_manager_v1 *manager,
		struct ext_workspace_group_handle_v1 *handle) {
	struct xdpw_screencast_context *ctx = data;
	struct xdpw_workspace_group *group = calloc(1, sizeof(*group));
	if (!group) {
		ext_workspace_group_handle_v1_destroy(handle);
		return;
	}
	group->ctx = ctx;
	group->handle = handle;
	wl_list_init(&group->outputs);
	wl_list_insert(&ctx->workspace_groups, &group->link);
	ext_workspace_group_handle_v1_add_listener(handle, &group_listener, group);
}

static void manager_finished(void *data, struct ext_workspace_manager_v1 *manager) {
	struct xdpw_screencast_context *ctx = data;
	xdpw_workspace_finish(ctx);
}

static void manager_done(void *data, struct ext_workspace_manager_v1 *manager) {
}

static const struct ext_workspace_manager_v1_listener manager_listener = {
	.workspace_group = manager_group,
	.workspace = manager_workspace,
	.done = manager_done,
	.finished = manager_finished,
};

void xdpw_workspace_bind(struct xdpw_screencast_context *ctx, struct wl_registry *registry,
		uint32_t id) {
	ctx->workspace_manager = wl_registry_bind(registry, id, &ext_workspace_manager_v1_interface, 1);
	ext_workspace_manager_v1_add_listener(ctx->workspace_manager, &manager_listener, ctx);
}

void xdpw_workspace_finish(struct xdpw_screencast_context *ctx) {
	struct xdpw_workspace *workspace, *tmp;
	wl_list_for_each_safe(workspace, tmp, &ctx->workspaces, link) {
		workspace_destroy(workspace);
	}
	struct xdpw_workspace_group *group, *group_tmp;
	wl_list_for_each_safe(group, group_tmp, &ctx->workspace_groups, link) {
		group_removed(group, group->handle);
	}
	if (ctx->workspace_manager) {
		// The manager has no destructor request; destroy the local proxy.
		ext_workspace_manager_v1_destroy(ctx->workspace_manager);
		ctx->workspace_manager = NULL;
	}
}
