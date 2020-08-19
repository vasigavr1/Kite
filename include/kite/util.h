#ifndef KITE_UTILS_H
#define KITE_UTILS_H

#include "multicast.h"
#include "kvs.h"
#include "hrd.h"
#include "main.h"
#include "../../../odlib/include/network_api/network_context.h"


extern uint64_t seed;
void kite_static_assert_compile_parameters();
void print_parameters_in_the_start();
void kite_init_globals();


/* ---------------------------------------------------------------------------
------------------------------STATS --------------------------------------
---------------------------------------------------------------------------*/
struct stats {
  double r_batch_size[WORKERS_PER_MACHINE];
  double r_rep_batch_size[WORKERS_PER_MACHINE];
  double ack_batch_size[WORKERS_PER_MACHINE];
  double write_batch_size[WORKERS_PER_MACHINE];
  double stalled_ack[WORKERS_PER_MACHINE];
  double stalled_r_rep[WORKERS_PER_MACHINE];
	double failed_rem_write[WORKERS_PER_MACHINE];
  double quorum_reads_per_thread[WORKERS_PER_MACHINE];

	double cache_hits_per_thread[WORKERS_PER_MACHINE];

	double writes_sent[WORKERS_PER_MACHINE];
	double reads_sent[WORKERS_PER_MACHINE];
	double acks_sent[WORKERS_PER_MACHINE];
	double proposes_sent[WORKERS_PER_MACHINE];
	double rmws_completed[WORKERS_PER_MACHINE];
	double accepts_sent[WORKERS_PER_MACHINE];
	double commits_sent[WORKERS_PER_MACHINE];

	double r_reps_sent[WORKERS_PER_MACHINE];
	double received_writes[WORKERS_PER_MACHINE];
	double received_reads[WORKERS_PER_MACHINE];
	double received_acks[WORKERS_PER_MACHINE];
	double received_r_reps[WORKERS_PER_MACHINE];
  double cancelled_rmws[WORKERS_PER_MACHINE];
	double all_aboard_rmws[WORKERS_PER_MACHINE];
	double reads_that_become_writes[WORKERS_PER_MACHINE];
  //double zookeeper_writes[WORKERS_PER_MACHINE];
};
void dump_stats_2_file(struct stats* st);
void print_latency_stats(void);


/* ---------------------------------------------------------------------------
-----------------------------------------------------------------------------
---------------------------------------------------------------------------*/

//Set up the depths of all QPs
void set_up_queue_depths(int**, int**);

void kite_init_qp_meta(context_t *ctx);
// Initialize the struct that holds all pending ops
p_ops_t* set_up_pending_ops(context_t *ctx);
// Set up the memory registrations in case inlining is disabled
// Set up the memory registrations required
void set_up_mr(struct ibv_mr **mr, void *buf, uint8_t enable_inlining, uint32_t buffer_size,
               hrd_ctrl_blk_t *cb);
// Set up all Broadcast WRs
void set_up_bcast_WRs(struct ibv_send_wr*, struct ibv_sge*,
                      struct ibv_send_wr*, struct ibv_sge*, uint16_t,
                      hrd_ctrl_blk_t*, struct ibv_mr*, struct ibv_mr*,
											mcast_cb_t* );
// Set up the r_rep replies and acks send and recv wrs
void set_up_ack_n_r_rep_WRs(struct ibv_send_wr*, struct ibv_sge*,
                            struct ibv_send_wr*, struct ibv_sge*,
                            hrd_ctrl_blk_t*, struct ibv_mr*,
                            ack_mes_t*, uint16_t);




// Post receives for the coherence traffic in the init phase
void pre_post_recvs(uint32_t*, struct ibv_qp *, uint32_t lkey, void*,
                    uint32_t, uint32_t, uint16_t, uint32_t);

// Set up the credits
void set_up_credits(uint16_t credits[][MACHINE_NUM]);

void randomize_op_values(trace_op_t *ops, uint16_t t_id);


/* ---------------------------------------------------------------------------
------------------------------UTILITY --------------------------------------
---------------------------------------------------------------------------*/
void print_latency_stats(void);


static mcast_cb_t* kite_init_multicast(hrd_ctrl_blk_t *cb, uint16_t t_id)
{
  uint32_t *recv_q_depth = (uint32_t *) malloc(MCAST_RECV_QP_NUM * sizeof(uint32_t));
  recv_q_depth[R_RECV_MCAST_QP] = RECV_R_Q_DEPTH;
  recv_q_depth[W_RECV_MCAST_QP] = RECV_W_Q_DEPTH;


	uint16_t recv_qp_num = MCAST_RECV_QP_NUM, send_num = MCAST_SEND_QP_NUM;

	uint16_t * group_to_send_to = (uint16_t *) malloc(MCAST_FLOW_NUM * (sizeof(uint16_t)));
	uint16_t *groups_per_flow = (uint16_t *) calloc(MCAST_FLOW_NUM, sizeof(uint16_t));
	for (int i = 0; i < MCAST_FLOW_NUM; ++i) {
		group_to_send_to[i] = (uint16_t) machine_id; // Which group you want to send to in that flow
		groups_per_flow[i] = MCAST_GROUPS_PER_FLOW;
	}

	bool *recvs_from_flow = (bool *) malloc(MCAST_FLOW_NUM * sizeof(bool));
	for (int i = 0; i < MCAST_FLOW_NUM; ++i) {
		recvs_from_flow[i] = true;
	}

	return create_mcast_cb(MCAST_FLOW_NUM, recv_qp_num, send_num,
												 groups_per_flow, recv_q_depth,
												 group_to_send_to,
												 recvs_from_flow,
												 local_ip,
												 (void *) cb->dgram_buf,
												 (size_t) TOTAL_BUF_SIZE, t_id);

}


#endif /* KITE_UTILS_H */
