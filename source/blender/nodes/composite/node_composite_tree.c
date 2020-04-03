/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup nodes
 */

#include <stdio.h>

#include "DNA_color_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_tracking.h"

#include "node_common.h"
#include "node_util.h"

#include "RNA_access.h"

#include "NOD_composite.h"
#include "node_composite_util.h"

#ifdef WITH_COMPOSITOR
#  include "COM_compositor.h"
#endif

static void composite_get_from_context(const bContext *C,
                                       bNodeTreeType *UNUSED(treetype),
                                       bNodeTree **r_ntree,
                                       ID **r_id,
                                       ID **r_from)
{
  Scene *scene = CTX_data_scene(C);

  *r_from = NULL;
  *r_id = &scene->id;
  *r_ntree = scene->nodetree;
}

static void foreach_nodeclass(Scene *UNUSED(scene), void *calldata, bNodeClassCallback func)
{
  func(calldata, NODE_CLASS_INPUT, N_("Input"));
  func(calldata, NODE_CLASS_OUTPUT, N_("Output"));
  func(calldata, NODE_CLASS_OP_COLOR, N_("Color"));
  func(calldata, NODE_CLASS_OP_VECTOR, N_("Vector"));
  func(calldata, NODE_CLASS_OP_FILTER, N_("Filter"));
  func(calldata, NODE_CLASS_CONVERTOR, N_("Convertor"));
  func(calldata, NODE_CLASS_MATTE, N_("Matte"));
  func(calldata, NODE_CLASS_DISTORT, N_("Distort"));
  func(calldata, NODE_CLASS_GROUP, N_("Group"));
  func(calldata, NODE_CLASS_INTERFACE, N_("Interface"));
  func(calldata, NODE_CLASS_LAYOUT, N_("Layout"));
}

static void free_node_cache(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sock;

  for (sock = node->outputs.first; sock; sock = sock->next) {
    if (sock->cache) {
      sock->cache = NULL;
    }
  }
}

static void free_cache(bNodeTree *ntree)
{
  bNode *node;
  for (node = ntree->nodes.first; node; node = node->next) {
    free_node_cache(ntree, node);
  }
}

/* local tree then owns all compbufs */
static void localize(bNodeTree *localtree, bNodeTree *ntree)
{

  bNode *node = ntree->nodes.first;
  bNode *local_node = localtree->nodes.first;
  while (node != NULL) {

    /* Ensure new user input gets handled ok. */
    node->need_exec = 0;
    local_node->original = node;

    /* move over the compbufs */
    /* right after ntreeCopyTree() oldsock pointers are valid */

    if (ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
      if (node->id) {
        if (node->flag & NODE_DO_OUTPUT) {
          local_node->id = (ID *)node->id;
        }
        else {
          local_node->id = NULL;
        }
      }
    }

    bNodeSocket *output_sock = node->outputs.first;
    bNodeSocket *local_output_sock = local_node->outputs.first;
    while (output_sock != NULL) {
      local_output_sock->cache = output_sock->cache;
      output_sock->cache = NULL;
      /* This is actually link to original: someone was just lazy enough and tried to save few
       * bytes in the cost of readability. */
      local_output_sock->new_sock = output_sock;

      output_sock = output_sock->next;
      local_output_sock = local_output_sock->next;
    }

    node = node->next;
    local_node = local_node->next;
  }
}

static void local_sync(bNodeTree *localtree, bNodeTree *ntree)
{
  BKE_node_preview_sync_tree(ntree, localtree);
}

static void local_merge(Main *bmain, bNodeTree *localtree, bNodeTree *ntree)
{
  bNode *lnode;
  bNodeSocket *lsock;

  /* move over the compbufs and previews */
  BKE_node_preview_merge_tree(ntree, localtree, true);

  for (lnode = localtree->nodes.first; lnode; lnode = lnode->next) {
    if (ntreeNodeExists(ntree, lnode->new_node)) {
      if (ELEM(lnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
        if (lnode->id && (lnode->flag & NODE_DO_OUTPUT)) {
          /* image_merge does sanity check for pointers */
          BKE_image_merge(bmain, (Image *)lnode->new_node->id, (Image *)lnode->id);
        }
      }
      else if (lnode->type == CMP_NODE_MOVIEDISTORTION) {
        /* special case for distortion node: distortion context is allocating in exec function
         * and to achieve much better performance on further calls this context should be
         * copied back to original node */
        if (lnode->storage) {
          if (lnode->new_node->storage) {
            BKE_tracking_distortion_free(lnode->new_node->storage);
          }

          lnode->new_node->storage = BKE_tracking_distortion_copy(lnode->storage);
        }
      }

      for (lsock = lnode->outputs.first; lsock; lsock = lsock->next) {
        if (ntreeOutputExists(lnode->new_node, lsock->new_sock)) {
          lsock->new_sock->cache = lsock->cache;
          lsock->cache = NULL;
          lsock->new_sock = NULL;
        }
      }
    }
  }
}

static void update(bNodeTree *ntree)
{
  ntreeSetOutput(ntree);

  ntree_update_reroute_nodes(ntree);

  if (ntree->update & NTREE_UPDATE_NODES) {
    /* clean up preview cache, in case nodes have been removed */
    BKE_node_preview_remove_unused(ntree);
  }
}

static void composite_node_add_init(bNodeTree *UNUSED(bnodetree), bNode *bnode)
{
  /* Composite node will only show previews for input classes
   * by default, other will be hidden
   * but can be made visible with the show_preview option */
  if (bnode->typeinfo->nclass != NODE_CLASS_INPUT) {
    bnode->flag &= ~NODE_PREVIEW;
  }
}

bNodeTreeType *ntreeType_Composite;

void register_node_tree_type_cmp(void)
{
  bNodeTreeType *tt = ntreeType_Composite = MEM_callocN(sizeof(bNodeTreeType),
                                                        "compositor node tree type");

  tt->type = NTREE_COMPOSIT;
  strcpy(tt->idname, "CompositorNodeTree");
  strcpy(tt->ui_name, N_("Compositor"));
  tt->ui_icon = 0; /* defined in drawnode.c */
  strcpy(tt->ui_description, N_("Compositing nodes"));

  tt->free_cache = free_cache;
  tt->free_node_cache = free_node_cache;
  tt->foreach_nodeclass = foreach_nodeclass;
  tt->localize = localize;
  tt->local_sync = local_sync;
  tt->local_merge = local_merge;
  tt->update = update;
  tt->get_from_context = composite_get_from_context;
  tt->node_add_init = composite_node_add_init;

  tt->ext.srna = &RNA_CompositorNodeTree;

  ntreeTypeAdd(tt);
}

extern void *COM_linker_hack; /* Quiet warning. */
void *COM_linker_hack = NULL;

void ntreeCompositExecTree(Scene *scene,
                           bNodeTree *ntree,
                           RenderData *rd,
                           int rendering,
                           int do_preview,
                           const ColorManagedViewSettings *view_settings,
                           const ColorManagedDisplaySettings *display_settings,
                           const char *view_name)
{
#ifdef WITH_COMPOSITOR
  COM_execute(rd, scene, ntree, rendering, view_settings, display_settings, view_name);
#else
  UNUSED_VARS(scene, ntree, rd, rendering, view_settings, display_settings, view_name);
#endif

  UNUSED_VARS(do_preview);
}

/* *********************************************** */

/* Update the outputs of the render layer nodes.
 * Since the outputs depend on the render engine, this part is a bit complex:
 * - ntreeCompositUpdateRLayers is called and loops over all render layer nodes.
 * - Each render layer node calls the update function of the
 *   render engine that's used for its scene.
 * - The render engine calls RE_engine_register_pass for each pass.
 * - RE_engine_register_pass calls ntreeCompositRegisterPass,.
 *   which calls node_cmp_rlayers_register_pass for every render layer node.
 */
void ntreeCompositUpdateRLayers(bNodeTree *ntree)
{
  bNode *node;

  if (ntree == NULL) {
    return;
  }

  for (node = ntree->nodes.first; node; node = node->next) {
    if (node->type == CMP_NODE_R_LAYERS) {
      node_cmp_rlayers_outputs(ntree, node);
    }
  }
}

void ntreeCompositRegisterPass(bNodeTree *ntree,
                               Scene *scene,
                               ViewLayer *view_layer,
                               const char *name,
                               eNodeSocketDatatype type)
{
  bNode *node;

  if (ntree == NULL) {
    return;
  }

  for (node = ntree->nodes.first; node; node = node->next) {
    if (node->type == CMP_NODE_R_LAYERS) {
      node_cmp_rlayers_register_pass(ntree, node, scene, view_layer, name, type);
    }
  }
}

/* called from render pipeline, to tag render input and output */
/* need to do all scenes, to prevent errors when you re-render 1 scene */
void ntreeCompositTagRender(Scene *curscene)
{
  Scene *sce;

  /* XXX Think using G_MAIN here is valid, since you want to update current file's scene nodes,
   * not the ones in temp main generated for rendering?
   * This is still rather weak though,
   * ideally render struct would store own main AND original G_MAIN. */

  for (sce = G_MAIN->scenes.first; sce; sce = sce->id.next) {
    if (sce->nodetree) {
      bNode *node;

      for (node = sce->nodetree->nodes.first; node; node = node->next) {
        if (node->id == (ID *)curscene || node->type == CMP_NODE_COMPOSITE) {
          nodeUpdate(sce->nodetree, node);
        }
        else if (node->type == CMP_NODE_TEXTURE) /* uses scene sizex/sizey */ {
          nodeUpdate(sce->nodetree, node);
        }
      }
    }
  }
}

/* XXX after render animation system gets a refresh, this call allows composite to end clean */
void ntreeCompositClearTags(bNodeTree *ntree)
{
  bNode *node;

  if (ntree == NULL) {
    return;
  }

  for (node = ntree->nodes.first; node; node = node->next) {
    node->need_exec = 0;
    if (node->type == NODE_GROUP) {
      ntreeCompositClearTags((bNodeTree *)node->id);
    }
  }
}
