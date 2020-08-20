#ifndef KITE_UTILS_H
#define KITE_UTILS_H


//#include "multicast.h"
#include "kvs.h"
#include "hrd.h"
#include "main.h"
#include "../../../odlib/include/network_api/network_context.h"
#include <init_func.h>

extern uint64_t seed;
void kite_static_assert_compile_parameters();
void kite_print_parameters_in_the_start();
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


void kite_init_qp_meta(context_t *ctx);
// Initialize the struct that holds all pending ops
p_ops_t* set_up_pending_ops(context_t *ctx);

void randomize_op_values(trace_op_t *ops, uint16_t t_id);


/* ---------------------------------------------------------------------------
------------------------------UTILITY --------------------------------------
---------------------------------------------------------------------------*/
void print_latency_stats(void);

static void kite_init_functionality(int argc, char *argv[])
{
  kite_print_parameters_in_the_start();
  generic_static_assert_compile_parameters();
  kite_static_assert_compile_parameters();
  kite_print_parameters_in_the_start();
  generic_init_globals(QP_NUM);
  kite_init_globals();
  handle_program_inputs(argc, argv);
}


#endif /* KITE_UTILS_H */
