#include "util.h"
#include "kite_inline_util.h"
#include "init_connect.h"
#include "trace_util.h"
#include "rdma_gen_util.h"

void *worker(void *arg)
{
	struct thread_params params = *(struct thread_params *) arg;
	uint16_t t_id = (uint16_t) params.id;
  if (ENABLE_MULTICAST && t_id == 0)
    my_printf(cyan, " Multicast is enabled \n");


  context_t *ctx = create_ctx((uint8_t) machine_id,
                              (uint16_t) params.id,
                              (uint16_t) QP_NUM,
                              local_ip);

  kite_init_qp_meta(ctx);
  set_up_ctx(ctx);

  /* -----------------------------------------------------
  --------------CONNECT -----------------------
  ---------------------------------------------------------*/
  setup_connections_and_spawn_stats_thread(ctx);
  init_ctx_send_wrs(ctx);

  ctx->appl_ctx = (void*) set_up_pending_ops(ctx);


	if (t_id == 0) my_printf(green, "Worker %d  reached the loop \n", t_id);


  main_loop(ctx);
	return NULL;
}

