#include "util.h"
#include "kite_inline_util.h"
#include "init_connect.h"
#include "trace_util.h"
#include "rdma_gen_util.h"

void *worker(void *arg)
{
	struct thread_params params = *(struct thread_params *) arg;
	uint16_t t_id = (uint16_t)params.id;
  uint32_t g_id = get_gid((uint8_t) machine_id, t_id);
  if (ENABLE_MULTICAST && t_id == 0)
    my_printf(cyan, " Multicast is enabled \n");


  context_t *ctx = create_ctx((uint8_t) machine_id,
                              (uint16_t) params.id,
                              (uint16_t) QP_NUM,
                              local_ip);

  per_qp_meta_t *qp_meta = ctx->qp_meta;
  create_per_qp_meta(&qp_meta[R_QP_ID], MAX_R_WRS,
                     MAX_RECV_R_WRS, SEND_BCAST_RECV_BCAST,  RECV_REQ,
                     REM_MACH_NUM, REM_MACH_NUM, R_BUF_SLOTS,
                     R_RECV_SIZE, R_SEND_SIZE, ENABLE_MULTICAST, ENABLE_MULTICAST,
                     R_SEND_MCAST_QP, 0, R_FIFO_SIZE,
                     R_CREDITS, R_MES_HEADER,
                     "send reads", "recv reads");

  ///
  create_per_qp_meta(&qp_meta[W_QP_ID], MAX_W_WRS,
                     MAX_RECV_W_WRS, SEND_BCAST_RECV_BCAST,  RECV_REQ,
                     REM_MACH_NUM, REM_MACH_NUM, W_BUF_SLOTS,
                     W_RECV_SIZE, W_SEND_SIZE, ENABLE_MULTICAST, ENABLE_MULTICAST,
                     W_SEND_MCAST_QP, 0, W_FIFO_SIZE,
                     W_CREDITS, W_MES_HEADER,
                     "send writes", "recv writes");
  ///
  create_per_qp_meta(&qp_meta[R_REP_QP_ID], MAX_R_REP_WRS,
                     MAX_RECV_R_REP_WRS, SEND_UNI_REP_RECV_UNI_REP, RECV_REPLY,
                     REM_MACH_NUM, REM_MACH_NUM, R_REP_BUF_SLOTS,
                     R_REP_RECV_SIZE, R_REP_SEND_SIZE, false, false,
                     0, 0, R_REP_FIFO_SIZE,
                     0, R_REP_MES_HEADER,
                     "send r_reps", "recv r_reps");
  ///

  ///
  create_per_qp_meta(&qp_meta[ACK_QP_ID], MAX_ACK_WRS, MAX_RECV_ACK_WRS,
                     SEND_UNI_REP_RECV_UNI_REP,
                     RECV_REPLY,
                     REM_MACH_NUM, REM_MACH_NUM, ACK_BUF_SLOTS,
                     ACK_RECV_SIZE, ACK_SIZE, false, false,
                     0, 0, 0, 0, 0,
                     "send acks", "recv acks");
  set_up_ctx(ctx);

  /* -----------------------------------------------------
  --------------CONNECT -----------------------
  ---------------------------------------------------------*/
  setup_connections_and_spawn_stats_thread(ctx->cb, t_id);
  init_ctx_send_wrs(ctx);

  ctx->appl_ctx = (void*) set_up_pending_ops(ctx);
  uint64_t r_br_tx = 0, w_br_tx = 0, r_rep_tx = 0, ack_tx = 0;
	uint32_t trace_iter = 0;



  ack_mes_t acks[MACHINE_NUM] = {0};
  for (uint16_t i = 0; i < MACHINE_NUM; i++) {
    acks[i].m_id = (uint8_t) machine_id;
    acks[i].opcode = OP_ACK;
    ctx->qp_meta[ACK_QP_ID].send_wr[i].sg_list->addr = (uintptr_t) &acks[i];
  }
  p_ops_t *p_ops = (p_ops_t *) ctx->appl_ctx;


  trace_op_t *ops = (trace_op_t *) calloc(MAX_OP_BATCH, sizeof(trace_op_t));
  randomize_op_values(ops, t_id);
  kv_resp_t *resp = (kv_resp_t *) malloc(MAX_OP_BATCH * sizeof(kv_resp_t));

	// TRACE
	trace_t *trace;
  if (!ENABLE_CLIENTS)
	  trace = trace_init(t_id);

	/* ---------------------------------------------------------------------------
	------------------------------LATENCY AND DEBUG-----------------------------------
	---------------------------------------------------------------------------*/
  latency_info_t latency_info = {
    .measured_req_flag = NO_REQ,
    .measured_sess_id = 0,
  };
  uint32_t waiting_dbg_counter[QP_NUM] = {0};
  uint32_t credit_debug_cnt[VC_NUM] = {0}, time_out_cnt[VC_NUM] = {0}, release_rdy_dbg_cnt = 0;
  struct session_dbg *ses_dbg = NULL;
  if (DEBUG_SESSIONS) {
    ses_dbg = (struct session_dbg *) malloc(sizeof(struct session_dbg));
    memset(ses_dbg, 0, sizeof(struct session_dbg));
  }
  uint16_t last_session = 0;
  uint32_t outstanding_writes = 0, outstanding_reads = 0, sizes_dbg_cntr;
  uint64_t debug_lids = 0;
  // helper for polling writes: in a corner failure-realted case,
  // it may be that not all avaialble writes can be polled due to the unavailability of the acks
  uint32_t completed_but_not_polled_writes = 0, loop_counter = 0;

	if (t_id == 0) my_printf(green, "Worker %d  reached the loop \n", t_id);
  bool slept = false;
  //fprintf(stderr, "Worker %d  reached the loop \n", t_id);

	/* ---------------------------------------------------------------------------
	------------------------------START LOOP--------------------------------
	---------------------------------------------------------------------------*/
	while(true) {
     //if (ENABLE_ASSERTIONS && CHECK_DBG_COUNTERS)
     //  check_debug_cntrs(credit_debug_cnt, waiting_dbg_counter, p_ops,
     //                    (void *) cb->dgram_buf, r_buf_pull_ptr,
     //                    w_buf_pull_ptr, ack_buf_pull_ptr, r_rep_buf_pull_ptr, t_id);



    if (PUT_A_MACHINE_TO_SLEEP && (machine_id == MACHINE_THAT_SLEEPS) &&
      (t_stats[WORKERS_PER_MACHINE -1].cache_hits_per_thread > 4000000) && (!slept)) {
      uint seconds = 15;
      if (t_id == 0) my_printf(yellow, "Workers are going to sleep for %u secs\n", seconds);
      sleep(seconds); slept = true;
      if (t_id == 0) my_printf(green, "Worker %u is back\n", t_id);
    }
    if (ENABLE_INFO_DUMP_ON_STALL && print_for_debug) {
      //print_verbouse_debug_info(p_ops, t_id, credits);
    }
    if (ENABLE_ASSERTIONS) {
      if (ENABLE_ASSERTIONS && t_id == 0)  time_approx++;
      loop_counter++;
      if (loop_counter == M_16) {
         //if (t_id == 0) print_all_stalled_sessions(p_ops, t_id);

        //printf("Wrkr %u is working rectified keys %lu \n",
        //       t_id, t_stats[t_id].rectified_keys);

//        if (t_id == 0) {
//          printf("Wrkr %u sleeping machine bit %u, q-reads %lu, "
//                   "epoch_id %u, reqs %lld failed writes %lu, writes done %lu/%lu \n", t_id,
//                 conf_bit_vec[MACHINE_THAT_SLEEPS].bit,
//                 t_stats[t_id].quorum_reads, (uint16_t) epoch_id,
//                 t_stats[t_id].cache_hits_per_thread, t_stats[t_id].failed_rem_writes,
//                 t_stats[t_id].writes_sent, t_stats[t_id].writes_asked_by_clients);
//        }
        loop_counter = 0;
      }
    }

    /* ---------------------------------------------------------------------------
		------------------------------ POLL FOR WRITES--------------------------
		---------------------------------------------------------------------------*/
    poll_for_writes(ctx, W_QP_ID, acks);

    /* ---------------------------------------------------------------------------
       ------------------------------ SEND ACKS----------------------------------
       ---------------------------------------------------------------------------*/

    send_acks(ctx, acks);

    /* ---------------------------------------------------------------------------
		------------------------------ POLL FOR READS--------------------------
		---------------------------------------------------------------------------*/
    poll_for_reads(ctx);

    /* ---------------------------------------------------------------------------
		------------------------------ SEND READ REPLIES--------------------------
		---------------------------------------------------------------------------*/

    send_r_reps(ctx);

    /* ---------------------------------------------------------------------------
		------------------------------ POLL FOR READ REPLIES--------------------------
		---------------------------------------------------------------------------*/

    poll_for_read_replies(ctx);

    /* ---------------------------------------------------------------------------
		------------------------------ COMMIT READS----------------------------------
		---------------------------------------------------------------------------*/
    // Either commit a read or convert it into a write
    commit_reads(p_ops, &latency_info, ctx->t_id);

    /* ---------------------------------------------------------------------------
		------------------------------ INSPECT RMWS----------------------------------
		---------------------------------------------------------------------------*/
    inspect_rmws(p_ops, ctx->t_id);

    /* ---------------------------------------------------------------------------
    ------------------------------ POLL FOR ACKS--------------------------------
    ---------------------------------------------------------------------------*/
    poll_acks(ctx);

    remove_writes(p_ops, &latency_info, ctx->t_id);

    /* ---------------------------------------------------------------------------
    ------------------------------PROBE THE CACHE--------------------------------------
    ---------------------------------------------------------------------------*/

    // Get a new batch from the trace, pass it through the kvs and create
    // the appropriate write/r_rep messages
    trace_iter = batch_requests_to_KVS(ctx, t_id,
                                       trace_iter, trace, ops,
                                       p_ops, resp, &latency_info,
                                       ses_dbg, &last_session, &sizes_dbg_cntr);
    /* ---------------------------------------------------------------------------
		------------------------------BROADCAST READS--------------------------
		---------------------------------------------------------------------------*/
    // Perform the r_rep broadcasts
    broadcast_reads(ctx);


   /* ---------------------------------------------------------------------------
		------------------------------BROADCAST WRITES--------------------------
		---------------------------------------------------------------------------*/
    // Perform the write broadcasts
    broadcast_writes(ctx, &release_rdy_dbg_cnt, &debug_lids);
	}
	return NULL;
}

