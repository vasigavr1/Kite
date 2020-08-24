#include <rdma_gen_util.h>
#include <trace_util.h>
#include "util.h"
#include "generic_inline_util.h"

struct bit_vector send_bit_vector;
struct multiple_owner_bit conf_bit_vec[MACHINE_NUM];

atomic_uint_fast64_t epoch_id;
atomic_bool print_for_debug;
const uint16_t machine_bit_id[SEND_CONF_VEC_SIZE * 8] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
                                                         1024, 2048, 4096, 8192, 16384, 32768};
atomic_uint_fast64_t committed_glob_sess_rmw_id[GLOBAL_SESSION_NUM];
FILE* rmw_verify_fp[WORKERS_PER_MACHINE];
FILE* client_log[CLIENTS_PER_MACHINE];


void kite_static_assert_compile_parameters()
{
  static_assert(RMW_ACK_BASE_TS_STALE > RMW_ACK, "assumption used to check if replies are acks");
  static_assert(PAXOS_TS > ALL_ABOARD_TS, "Paxos TS must be bigger than ALL Aboard TS");
  static_assert(!(COMMIT_LOGS && (PRINT_LOGS || VERIFY_PAXOS)), " ");
  static_assert(sizeof(struct key) == KEY_SIZE, " ");
  static_assert(sizeof(struct network_ts_tuple) == TS_TUPLE_SIZE, "");
//  static_assert(sizeof(struct cache_key) ==  KEY_SIZE, "");
//  static_assert(sizeof(cache_meta) == 8, "");

 static_assert(INVALID_RMW == 0, "the initial state of a mica_op must be invalid");
  static_assert(MACHINE_NUM < 16, "the bit_vec vector is 16 bits-- can be extended");

  static_assert(VALUE_SIZE >= 2, "first round of release can overload the first 2 bytes of value");
  //static_assert(VALUE_SIZE > RMW_BYTE_OFFSET, "");
  static_assert(VALUE_SIZE == (RMW_VALUE_SIZE), "RMW requires the value to be at least this many bytes");
  static_assert(MACHINE_NUM <= 255, ""); // for deprecated reasons

  // WRITES
  static_assert(W_SEND_SIZE >= W_MES_SIZE &&
                W_SEND_SIZE >= ACC_MES_SIZE &&
                W_SEND_SIZE >= COM_MES_SIZE &&
                W_SEND_SIZE <= MAX_WRITE_SIZE, "");
  static_assert(W_SEND_SIZE <= MTU, "");
  // READS
  static_assert(R_SEND_SIZE >= R_MES_SIZE &&
                R_SEND_SIZE >= PROP_MES_SIZE &&
                R_SEND_SIZE <= MAX_READ_SIZE, "");
  static_assert(R_SEND_SIZE <= MTU, "");
  // R_REPS
//  static_assert(R_REP_SEND_SIZE >= PROP_REP_MES_SIZE &&
//                R_REP_SEND_SIZE >= ACC_REP_MES_SIZE &&
//                R_REP_SEND_SIZE >= READ_TS_REP_MES_SIZE &&
//                R_REP_SEND_SIZE >= ACQ_REP_MES_SIZE, "");
  static_assert(R_REP_SEND_SIZE <= MTU, "");

  // COALESCING
  static_assert(MAX_WRITE_COALESCE < 256, "");
  static_assert(MAX_READ_COALESCE < 256, "");
  static_assert(MAX_REPS_IN_REP < 256, "");
  static_assert(PROP_COALESCE > 0, "");
  static_assert(R_COALESCE > 0, "");
  static_assert(W_COALESCE > 0, "");
  static_assert(ACC_COALESCE > 0, "");
  static_assert(COM_COALESCE > 0, "");

  // NETWORK STRUCTURES
  static_assert(sizeof(struct read) == R_SIZE, "");
  static_assert(sizeof(struct r_message) == R_MES_SIZE, "");
  static_assert(sizeof(write_t) == W_SIZE, "");
  static_assert(sizeof(struct w_message) == W_MES_SIZE, "");
  static_assert(sizeof(struct propose) == PROP_SIZE, "");
  static_assert(PROP_REP_ACCEPTED_SIZE == PROP_REP_LOG_TOO_LOW_SIZE + 1, "");
  static_assert(sizeof(struct rmw_rep_last_committed) == PROP_REP_ACCEPTED_SIZE, "");
  static_assert(sizeof(struct rmw_rep_message) == PROP_REP_MES_SIZE, "");
  static_assert(sizeof(struct accept) == ACCEPT_SIZE, "");
  static_assert(sizeof(struct r_rep_big) == ACQ_REP_SIZE, "");
  static_assert(sizeof(struct commit) == COMMIT_SIZE, "");
  // UD- REQS
  static_assert(sizeof(r_rep_mes_ud_t) == R_REP_RECV_SIZE, "");
  static_assert(sizeof(ack_mes_ud_t) == ACK_RECV_SIZE, "");
  static_assert(sizeof(w_mes_ud_t) == W_RECV_SIZE, "");
  static_assert(sizeof(r_mes_ud_t) == R_RECV_SIZE, "");

  // we want to have more write slots than credits such that we always know that if a machine fails
  // the pressure will appear in the credits and not the write slots
  static_assert(PENDING_WRITES > (W_CREDITS * MAX_MES_IN_WRITE), " ");
  // RMWs
  static_assert(!ENABLE_RMWS || LOCAL_PROP_NUM >= SESSIONS_PER_THREAD, "");
  static_assert(GLOBAL_SESSION_NUM < K_64, "global session ids are stored in uint16_t");

  // ACCEPT REPLIES MAP TO PROPOSE REPLIES
  static_assert(ACC_REP_SIZE == PROP_REP_LOG_TOO_LOW_SIZE, "");
  static_assert(ACC_REP_SMALL_SIZE == PROP_REP_SMALL_SIZE, "");
  static_assert(ACC_REP_ONLY_TS_SIZE == PROP_REP_ONLY_TS_SIZE, "");
  //static_assert(ACC_REP_ACCEPTED_SIZE == PROP_REP_ACCEPTED_SIZE, "");



  static_assert(!(VERIFY_PAXOS && PRINT_LOGS), "only one of those can be set");
#if VERIFY_PAXOS == 1
  static_assert(EXIT_ON_PRINT == 1, "");
#endif
  //static_assert(sizeof(trace_op_t) == 18 + VALUE_SIZE  + 8 + 4, "");
  static_assert(TRACE_ONLY_CAS + TRACE_ONLY_FA + TRACE_MIXED_RMWS == 1, "");


//  printf("Client op  %u  %u \n", sizeof(client_op_t), PADDING_BYTES_CLIENT_OP);
//  printf("Interface \n \n %u  \n \n", sizeof(struct wrk_clt_if));

  static_assert(!(ENABLE_CLIENTS && !ACCEPT_IS_RELEASE && CLIENT_MODE > CLIENT_UI),
                "If we are using the lock-free data structures rmws must act as releases");

  static_assert(!(ENABLE_CLIENTS && CLIENT_MODE > CLIENT_UI && ENABLE_ALL_ABOARD),
                "All-aboard does not work with the RC semantics");
}

void kite_print_parameters_in_the_start()
{


  printf("MICA OP capacity %ld/%d added padding %d  \n",
         sizeof(mica_op_t), MICA_OP_SIZE, MICA_OP_PADDING_SIZE);


  printf("Rmw-local_entry %ld \n", sizeof(loc_entry_t));
  printf("quorum-num %d \n", QUORUM_NUM);

  my_printf(green, "READ REPLY: r_rep message %lu/%d, r_rep message ud req %llu/%d,"
                 "read info %llu\n",
               sizeof(struct r_rep_message), R_REP_SEND_SIZE,
               sizeof(r_rep_mes_ud_t), R_REP_RECV_SIZE,
               sizeof (r_info_t));
  my_printf(green, "W_COALESCE %d, R_COALESCE %d, ACC_COALESCE %u, "
                 "PROPOSE COALESCE %d, COM_COALESCE %d, MAX_WRITE_COALESCE %d,"
                 "MAX_READ_COALESCE %d \n",
               W_COALESCE, R_COALESCE, ACC_COALESCE, PROP_COALESCE, COM_COALESCE,
               MAX_WRITE_COALESCE, MAX_READ_COALESCE);


  my_printf(cyan, "ACK: ack message %lu/%d, ack message ud req %llu/%d\n",
              sizeof(ack_mes_t), ACK_SIZE,
              sizeof(ack_mes_ud_t), ACK_RECV_SIZE);
  my_printf(yellow, "READ: read %lu/%d, read message %lu/%d, read message ud req %lu/%d\n",
                sizeof(struct read), R_SIZE,
                sizeof(struct r_message), R_SEND_SIZE,
                sizeof(r_mes_ud_t), R_RECV_SIZE);
  my_printf(cyan, "Write: write %lu/%d, write message %lu/%d, write message ud req %llu/%d\n",
              sizeof(write_t), W_SIZE,
              sizeof(struct w_message), W_SEND_SIZE,
              sizeof(w_mes_ud_t), W_RECV_SIZE);

  my_printf(green, "W INLINING %d, PENDING WRITES %d \n",
               W_ENABLE_INLINING, PENDING_WRITES);
  my_printf(green, "R INLINING %d, PENDING_READS %d \n",
               R_ENABLE_INLINING, PENDING_READS);
  my_printf(green, "R_REP INLINING %d \n",
               R_REP_ENABLE_INLINING);
  my_printf(cyan, "W CREDITS %d, W BUF SLOTS %d, W BUF SIZE %d\n",
              W_CREDITS, W_BUF_SLOTS, W_BUF_SIZE);

  my_printf(yellow, "Remote Quorum Machines %d \n", REMOTE_QUORUM);
  my_printf(green, "SEND W DEPTH %d, MESSAGES_IN_BCAST_BATCH %d, W_BCAST_SS_BATCH %d \n",
               SEND_W_Q_DEPTH, MESSAGES_IN_BCAST_BATCH, W_BCAST_SS_BATCH);
}

void kite_init_globals()
{
  uint32_t i = 0;
  epoch_id = MEASURE_SLOW_PATH ? 1 : 0;
  // This (sadly) seems to be the only way to initialize the locks
  // in struct_bit_vector, i.e. the atomic_flags
  memset(&send_bit_vector, 0, sizeof(struct bit_vector));
  memset(conf_bit_vec, 0, MACHINE_NUM * sizeof(struct multiple_owner_bit));

  for (i = 0; i < MACHINE_NUM; i++) {
    conf_bit_vec[i].bit = UP_STABLE;
    send_bit_vector.bit_vec[i].bit = UP_STABLE;
  }
  send_bit_vector.state = UP_STABLE;
  memset(committed_glob_sess_rmw_id, 0, GLOBAL_SESSION_NUM * sizeof(uint64_t));
}





void dump_stats_2_file(struct stats* st){
    uint8_t typeNo = 0;
    assert(typeNo >=0 && typeNo <=3);
    int i = 0;
    char filename[128];
    FILE *fp;
    double total_MIOPS;
    char* path = "../../results/scattered-results";

    sprintf(filename, "%s/%s-%s_s_%d_v_%d_m_%d_w_%d_r_%d-%d.csv", path,
            EMULATE_ABD == 1 ? "DRF-" : "REAL-",
            "ABD",
            SESSIONS_PER_THREAD,
            USE_BIG_OBJECTS == 1 ? ((EXTRA_CACHE_LINES * 64) + BASE_VALUE_SIZE): BASE_VALUE_SIZE,
            MACHINE_NUM, WORKERS_PER_MACHINE,
            WRITE_RATIO,
            machine_id);
    printf("%s\n", filename);
    fp = fopen(filename, "w"); // "w" means that we are going to write on this file
    fprintf(fp, "machine_id: %d\n", machine_id);

    fprintf(fp, "comment: thread ID, total MIOPS,"
            "reads sent, read-replies sent, acks sent, "
            "received read-replies, received reads, received acks\n");
    for(i = 0; i < WORKERS_PER_MACHINE; ++i){
        total_MIOPS = st->cache_hits_per_thread[i];
        fprintf(fp, "client: %d, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f\n",
                i, total_MIOPS, st->cache_hits_per_thread[i], st->reads_sent[i],
                st->r_reps_sent[i], st->acks_sent[i],
                st->received_r_reps[i],st->received_reads[i],
                st->received_acks[i]);
    }

    fclose(fp);
}

// If reading CAS rmws out of the trace, CASes that compare against 0 succeed the rest fail
void randomize_op_values(trace_op_t *ops, uint16_t t_id)
{
  if (!ENABLE_CLIENTS) {
    for (uint16_t i = 0; i < MAX_OP_BATCH; i++) {
      if (TRACE_ONLY_FA)
        ops[i].value[0] = 1;
      else if (rand() % 1000 < RMW_CAS_CANCEL_RATIO)
        memset(ops[i].value, 1, VALUE_SIZE);
      else memset(ops[i].value, 0, VALUE_SIZE);
    }
  }
}


/* ---------------------------------------------------------------------------
------------------------------KITE WORKER --------------------------------------
---------------------------------------------------------------------------*/

void kite_init_qp_meta(context_t *ctx)
{
  per_qp_meta_t *qp_meta = ctx->qp_meta;
  create_per_qp_meta(&qp_meta[R_QP_ID], MAX_R_WRS,
                     MAX_RECV_R_WRS, SEND_BCAST_RECV_BCAST,  RECV_REQ,
                     R_REP_QP_ID,
                     REM_MACH_NUM, REM_MACH_NUM, R_BUF_SLOTS,
                     R_RECV_SIZE, R_SEND_SIZE, ENABLE_MULTICAST, ENABLE_MULTICAST,
                     R_SEND_MCAST_QP, 0, R_FIFO_SIZE,
                     R_CREDITS, R_MES_HEADER,
                     "send reads", "recv reads");

  ///
  create_per_qp_meta(&qp_meta[W_QP_ID], MAX_W_WRS,
                     MAX_RECV_W_WRS, SEND_BCAST_RECV_BCAST,  RECV_REQ,
                     ACK_QP_ID,
                     REM_MACH_NUM, REM_MACH_NUM, W_BUF_SLOTS,
                     W_RECV_SIZE, W_SEND_SIZE, ENABLE_MULTICAST, ENABLE_MULTICAST,
                     W_SEND_MCAST_QP, 0, W_FIFO_SIZE,
                     W_CREDITS, W_MES_HEADER,
                     "send writes", "recv writes");
  ///
  create_per_qp_meta(&qp_meta[R_REP_QP_ID], MAX_R_REP_WRS,
                     MAX_RECV_R_REP_WRS, SEND_UNI_REP_RECV_UNI_REP, RECV_REPLY,
                     R_QP_ID,
                     REM_MACH_NUM, REM_MACH_NUM, R_REP_BUF_SLOTS,
                     R_REP_RECV_SIZE, R_REP_SEND_SIZE, false, false,
                     0, 0, R_REP_FIFO_SIZE,
                     0, R_REP_MES_HEADER,
                     "send r_reps", "recv r_reps");
  ///
  crate_ack_qp_meta(&qp_meta[ACK_QP_ID],
                    W_QP_ID, REM_MACH_NUM,
                    REM_MACH_NUM, W_CREDITS);
}

kite_debug_t *init_debug_loop_struct()
{
  kite_debug_t *loop_dbg = calloc(1, sizeof(kite_debug_t));
  if (DEBUG_SESSIONS) {
    loop_dbg->ses_dbg = (struct session_dbg *) malloc(sizeof(struct session_dbg));
    memset(loop_dbg->ses_dbg, 0, sizeof(struct session_dbg));
  }
  return loop_dbg;
}
// Initialize the pending ops struct
p_ops_t* set_up_pending_ops(context_t *ctx)
{
  uint32_t pending_reads = (uint32_t) PENDING_READS;
  uint32_t pending_writes = (uint32_t) PENDING_WRITES;
  uint32_t i, j;
  p_ops_t *p_ops = (p_ops_t *) calloc(1, sizeof(p_ops_t));


  //p_ops->w_state = (uint8_t *) malloc(zk_ctx * sizeof(uint8_t *));
  p_ops->r_state = (uint8_t *) malloc(pending_reads * sizeof(uint8_t *));
  //p_ops->w_session_id = (uint32_t *) calloc(zk_ctx, sizeof(uint32_t));
  p_ops->r_session_id = (uint32_t *) calloc(pending_reads, sizeof(uint32_t));
  p_ops->w_index_to_req_array = (uint32_t *) calloc(pending_writes, sizeof(uint32_t));
  p_ops->r_index_to_req_array = (uint32_t *) calloc(pending_reads, sizeof(uint32_t));
  //p_ops->session_has_pending_op = (bool *) calloc(SESSIONS_PER_THREAD, sizeof(bool));
  //p_ops->acks_seen = (uint8_t *) calloc(zk_ctx, sizeof(uint8_t));
  p_ops->read_info = (r_info_t *) calloc(pending_reads, sizeof(r_info_t));
  p_ops->p_ooe_writes =
    (struct pending_out_of_epoch_writes *) calloc(1, sizeof(struct pending_out_of_epoch_writes));


  // R_REP_FIFO
  p_ops->r_rep_fifo = (struct r_rep_fifo *) calloc(1, sizeof(struct r_rep_fifo));
  fifo_t *r_rep_send_fifo = ctx->qp_meta[R_REP_QP_ID].send_fifo;
  assert(r_rep_send_fifo->max_byte_size == R_REP_FIFO_SIZE * ALIGNED_R_REP_SEND_SIDE);
  p_ops->r_rep_fifo->r_rep_message = (struct r_rep_message_template *) r_rep_send_fifo->fifo;
    //(struct r_rep_message_template *) calloc((size_t)R_REP_FIFO_SIZE, (size_t)ALIGNED_R_REP_SEND_SIDE);
  p_ops->r_rep_fifo->rem_m_id = (uint8_t *) malloc(R_REP_FIFO_SIZE * sizeof(uint8_t));
  p_ops->r_rep_fifo->pull_ptr = 1;
  for (i= 0; i < R_REP_FIFO_SIZE; i++) p_ops->r_rep_fifo->rem_m_id[i] = MACHINE_NUM;
  p_ops->r_rep_fifo->message_sizes = (uint16_t *) calloc((size_t) R_REP_FIFO_SIZE, sizeof(uint16_t));

  // W_FIFO
  p_ops->w_fifo = (write_fifo_t *) calloc(1, sizeof(write_fifo_t));
  fifo_t *w_send_fifo = ctx->qp_meta[W_QP_ID].send_fifo;
  assert(w_send_fifo->max_byte_size == W_FIFO_SIZE * ALIGNED_W_SEND_SIDE);
  p_ops->w_fifo->w_message = (struct w_message_template *) w_send_fifo->fifo;
    //(struct w_message_template *) calloc((size_t)W_FIFO_SIZE, (size_t) ALIGNED_W_SEND_SIDE);

  // R_FIFO
  p_ops->r_fifo = (struct read_fifo *) calloc(1, sizeof(struct read_fifo));
  fifo_t *r_send_fifo = ctx->qp_meta[R_QP_ID].send_fifo;
  assert(r_send_fifo->max_byte_size == R_FIFO_SIZE * ALIGNED_R_SEND_SIDE);
  p_ops->r_fifo->r_message = (struct r_message_template *) r_send_fifo->fifo; //calloc(R_FIFO_SIZE, (size_t) ALIGNED_R_SEND_SIDE);

  ack_mes_t *ack_send_buf = (ack_mes_t *) ctx->qp_meta[ACK_QP_ID].send_fifo->fifo; //calloc(MACHINE_NUM, sizeof(ack_mes_t));
  assert(ctx->qp_meta[ACK_QP_ID].send_fifo->max_byte_size == ACK_SIZE * MACHINE_NUM);
  memset(ack_send_buf, 0, ctx->qp_meta[ACK_QP_ID].send_fifo->max_byte_size);
  for (i = 0; i < MACHINE_NUM; i++) {
    ack_send_buf[i].m_id = (uint8_t) machine_id;
    ack_send_buf[i].opcode = OP_ACK;
  }


  // PREP STRUCT
  p_ops->prop_info = (struct prop_info *)malloc(sizeof(struct prop_info));
  memset(p_ops->prop_info, 0, sizeof(struct prop_info));
  //assert(IS_ALIGNED(p_ops->prop_info, 64));
  for (i = 0; i < LOCAL_PROP_NUM; i++) {
    loc_entry_t *loc_entry = &p_ops->prop_info->entry[i];
    loc_entry->sess_id = (uint16_t) i;
    loc_entry->glob_sess_id = get_glob_sess_id((uint8_t)machine_id, ctx->t_id, (uint16_t) i);
    loc_entry->l_id = (uint64_t) loc_entry->sess_id;
    loc_entry->rmw_id.id = (uint64_t) loc_entry->glob_sess_id;
    loc_entry->help_rmw = (struct rmw_help_entry *) calloc(1, sizeof(struct rmw_help_entry));
    loc_entry->help_loc_entry = (loc_entry_t *) calloc(1, sizeof(loc_entry_t));
    loc_entry->help_loc_entry->sess_id = (uint16_t) i;
    loc_entry->help_loc_entry->helping_flag = IS_HELPER;
    loc_entry->help_loc_entry->glob_sess_id = loc_entry->glob_sess_id;
    loc_entry->state = INVALID_RMW;
  }
  p_ops->sess_info = (sess_info_t *) calloc(SESSIONS_PER_THREAD, sizeof(sess_info_t));
  p_ops->w_meta = (per_write_meta_t *) calloc(pending_writes, sizeof(per_write_meta_t));



   uint32_t max_incoming_w_r = (uint32_t) MAX(MAX_INCOMING_R, MAX_INCOMING_W);
  p_ops->ptrs_to_mes_headers =
    (struct r_message **) malloc(max_incoming_w_r * sizeof(struct r_message *));
  p_ops->coalesce_r_rep =
    (bool *) malloc(max_incoming_w_r* sizeof(bool));

  // PTRS to R_OPS
  p_ops->ptrs_to_mes_ops = (void **) malloc(max_incoming_w_r * sizeof(struct read *));
  // PTRS to local ops to find the write after sending the first round of a release
  p_ops->ptrs_to_local_w = (write_t **) malloc(pending_writes * sizeof(write_t *));
  p_ops->overwritten_values = (uint8_t *) calloc(pending_writes, SEND_CONF_VEC_SIZE);


  for (i = 0; i < SESSIONS_PER_THREAD; i++) {
    p_ops->sess_info[i].ready_to_release = true;
  }
  for (i = 0; i < W_FIFO_SIZE; i++) {
    struct w_message *w_mes = (struct w_message *) &p_ops->w_fifo->w_message[i];
    w_mes->m_id = (uint8_t) machine_id;
  }
  p_ops->w_fifo->info[0].message_size = W_MES_HEADER;

  for (i = 0; i < R_FIFO_SIZE; i++) {
    struct r_message *r_mes = (struct r_message *) &p_ops->r_fifo->r_message[i];
    r_mes->m_id= (uint8_t) machine_id;
  }
  p_ops->r_fifo->info[0].message_size = R_MES_HEADER;

  for (i = 0; i < R_REP_FIFO_SIZE; i++) {
    struct rmw_rep_message *rmw_mes = (struct rmw_rep_message *) &p_ops->r_rep_fifo->r_rep_message[i];
    assert(((void *)rmw_mes - (void *)p_ops->r_rep_fifo->r_rep_message) % ALIGNED_R_REP_SEND_SIDE == 0);
    rmw_mes->m_id = (uint8_t) machine_id;
  }
  for (i = 0; i < pending_reads; i++)
    p_ops->r_state[i] = INVALID;
  for (i = 0; i < pending_writes; i++) {
    p_ops->w_meta[i].w_state = INVALID;
    p_ops->ptrs_to_local_w[i] = NULL;
  }


  p_ops->ops = (trace_op_t *) calloc(MAX_OP_BATCH, sizeof(trace_op_t));
  randomize_op_values(p_ops->ops, ctx->t_id);
  p_ops->resp = (kv_resp_t *) malloc(MAX_OP_BATCH * sizeof(kv_resp_t));
  if (!ENABLE_CLIENTS)
    p_ops->trace = trace_init(ctx->t_id);

  p_ops->debug_loop = init_debug_loop_struct();
 return p_ops;
}





