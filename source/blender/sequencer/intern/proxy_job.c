/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved.
 *           2003-2009 Blender Foundation.
 *           2005-2006 Peter Schlaile <peter [at] schlaile [dot] de> */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_timecode.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "SEQ_iterator.h"
#include "SEQ_proxy.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

static void proxy_freejob(void *pjv)
{
  ProxyJob *pj = pjv;

  BLI_freelistN(&pj->queue);

  MEM_freeN(pj);
}

/* Only this runs inside thread. */
static void proxy_startjob(void *pjv, bool *stop, bool *do_update, float *progress)
{
  ProxyJob *pj = pjv;
  LinkData *link;

  for (link = pj->queue.first; link; link = link->next) {
    struct SeqIndexBuildContext *context = link->data;

    SEQ_proxy_rebuild(context, stop, do_update, progress);

    if (*stop) {
      pj->stop = true;
      fprintf(stderr, "Canceling proxy rebuild on users request...\n");
      break;
    }
  }
}

static void proxy_endjob(void *pjv)
{
  ProxyJob *pj = pjv;
  Editing *ed = SEQ_editing_get(pj->scene);
  LinkData *link;

  for (link = pj->queue.first; link; link = link->next) {
    SEQ_proxy_rebuild_finish(link->data, pj->stop);
  }

  SEQ_relations_free_imbuf(pj->scene, &ed->seqbase, false);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, pj->scene);
}

ProxyJob *ED_seq_proxy_job_get(const bContext *C, wmJob *wm_job)
{
  Scene *scene = CTX_data_scene(C);
  struct Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  ProxyJob *pj = WM_jobs_customdata_get(wm_job);
  if (!pj) {
    pj = MEM_callocN(sizeof(ProxyJob), "proxy rebuild job");
    pj->depsgraph = depsgraph;
    pj->scene = scene;
    pj->main = CTX_data_main(C);
    WM_jobs_customdata_set(wm_job, pj, proxy_freejob);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_SEQUENCER, NC_SCENE | ND_SEQUENCER);
    WM_jobs_callbacks(wm_job, proxy_startjob, NULL, NULL, proxy_endjob);
  }
  return pj;
}

struct wmJob *ED_seq_proxy_wm_job_get(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene,
                              "Building Proxies",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_SEQ_BUILD_PROXY);
  return wm_job;
}
