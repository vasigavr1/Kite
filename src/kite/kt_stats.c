#include "kt_util.h"

void print_latency_stats(void);

void kite_stats(stats_ctx_t *ctx)
{

  double seconds = ctx-> seconds;
  uint16_t print_count = ctx->print_count;
  t_stats_t *curr_w_stats = ctx->curr_w_stats;
  t_stats_t *prev_w_stats = ctx->prev_w_stats;
  c_stats_t *curr_c_stats = ctx->curr_c_stats;
  c_stats_t *prev_c_stats = ctx->prev_c_stats;
  all_stats_t all_stats;

  double total_throughput = 0;
  uint64_t all_clients_cache_hits = 0,
    all_wrkr_completed_reqs = 0,
    all_wrkr_completed_zk_writes = 0,
    all_wrkr_sync_percentage = 0;

  seconds *= MILLION;//1000; // compute only MIOPS
  uint64_t total_cancelled_rmws =  0, total_rmws = 0, total_all_aboard_rmws = 0;
  for (int i = 0; i < WORKERS_PER_MACHINE; i++) {

    all_wrkr_completed_reqs += curr_w_stats[i].cache_hits_per_thread - prev_w_stats[i].cache_hits_per_thread;
    all_wrkr_completed_zk_writes += (curr_w_stats[i].rmws_completed + curr_w_stats[i].releases_per_thread +
                                     curr_w_stats[i].writes_per_thread - (prev_w_stats[i].rmws_completed + prev_w_stats[i].releases_per_thread +
                                                                          prev_w_stats[i].writes_per_thread));

    all_wrkr_sync_percentage += (curr_w_stats[i].rmws_completed + curr_w_stats[i].releases_per_thread +
                                 curr_w_stats[i].acquires_per_thread  -
                                 (prev_w_stats[i].rmws_completed + prev_w_stats[i].releases_per_thread +
                                  prev_w_stats[i].acquires_per_thread));


    total_cancelled_rmws += curr_w_stats[i].cancelled_rmws - prev_w_stats[i].cancelled_rmws;
    total_rmws += curr_w_stats[i].rmws_completed - prev_w_stats[i].rmws_completed;
    total_all_aboard_rmws += curr_w_stats[i].all_aboard_rmws - prev_w_stats[i].all_aboard_rmws;

    all_stats.cache_hits_per_thread[i] =
      (curr_w_stats[i].cache_hits_per_thread - prev_w_stats[i].cache_hits_per_thread) / seconds;


    all_stats.stalled_ack[i] = (curr_w_stats[i].stalled_ack - prev_w_stats[i].stalled_ack) / seconds;
    all_stats.stalled_r_rep[i] =
      (curr_w_stats[i].stalled_r_rep - prev_w_stats[i].stalled_r_rep) / seconds;
    all_stats.reads_sent[i] = (curr_w_stats[i].reads_sent - prev_w_stats[i].reads_sent) / (seconds);
    all_stats.rmws_completed[i] = (curr_w_stats[i].rmws_completed - prev_w_stats[i].rmws_completed) / (seconds);
    all_stats.all_aboard_rmws[i] = (curr_w_stats[i].all_aboard_rmws - prev_w_stats[i].all_aboard_rmws) / (seconds);
    all_stats.proposes_sent[i] = (curr_w_stats[i].proposes_sent - prev_w_stats[i].proposes_sent) / (seconds);
    all_stats.accepts_sent[i] = (curr_w_stats[i].accepts_sent - prev_w_stats[i].accepts_sent) / (seconds);
    all_stats.commits_sent[i] = (curr_w_stats[i].commits_sent - prev_w_stats[i].commits_sent) / (seconds);

    all_stats.quorum_reads_per_thread[i] = (curr_w_stats[i].quorum_reads - prev_w_stats[i].quorum_reads) / (seconds);

    all_stats.writes_sent[i] = (curr_w_stats[i].writes_sent - prev_w_stats[i].writes_sent) / (seconds);
    all_stats.r_reps_sent[i] = (curr_w_stats[i].r_reps_sent - prev_w_stats[i].r_reps_sent) / seconds;
    all_stats.acks_sent[i] = (curr_w_stats[i].acks_sent - prev_w_stats[i].acks_sent) / seconds;
    all_stats.received_writes[i] = (curr_w_stats[i].received_writes - prev_w_stats[i].received_writes) / seconds;
    all_stats.received_reads[i] = (curr_w_stats[i].received_reads - prev_w_stats[i].received_reads) / seconds;
    all_stats.received_r_reps[i] = (curr_w_stats[i].received_r_reps - prev_w_stats[i].received_r_reps) / seconds;
    all_stats.received_acks[i] = (curr_w_stats[i].received_acks - prev_w_stats[i].received_acks) / seconds;

    all_stats.ack_batch_size[i] = (curr_w_stats[i].acks_sent - prev_w_stats[i].acks_sent) /
                                  (double) (curr_w_stats[i].acks_sent_mes_num -
                                            prev_w_stats[i].acks_sent_mes_num);
    all_stats.write_batch_size[i] = (curr_w_stats[i].writes_sent - prev_w_stats[i].writes_sent) /
                                    (double) (curr_w_stats[i].writes_sent_mes_num -
                                              prev_w_stats[i].writes_sent_mes_num);
    all_stats.r_batch_size[i] = (curr_w_stats[i].reads_sent - prev_w_stats[i].reads_sent) /
                                (double) (curr_w_stats[i].reads_sent_mes_num -
                                          prev_w_stats[i].reads_sent_mes_num);
    all_stats.r_rep_batch_size[i] = (curr_w_stats[i].r_reps_sent - prev_w_stats[i].r_reps_sent) /
                                    (double) (curr_w_stats[i].r_reps_sent_mes_num -
                                              prev_w_stats[i].r_reps_sent_mes_num);
    all_stats.failed_rem_write[i] = (curr_w_stats[i].failed_rem_writes - prev_w_stats[i].failed_rem_writes) /
                                    (double) (curr_w_stats[i].received_writes -
                                              prev_w_stats[i].received_writes);

    uint64_t curr_true_read_sent = curr_w_stats[i].reads_sent - curr_w_stats[i].releases_per_thread; // TODO this does not account for out-of-epoch writes
    uint64_t prev_true_read_sent = prev_w_stats[i].reads_sent - prev_w_stats[i].releases_per_thread;
    all_stats.reads_that_become_writes[i] = (curr_w_stats[i].read_to_write - prev_w_stats[i].read_to_write) /
                                            (double)((curr_w_stats[i].reads_sent - prev_w_stats[i].reads_sent));

  }

  uint64_t all_client_microbench_pushes = 0, all_client_microbench_pops = 0;
  for (int i = 0; i < CLIENTS_PER_MACHINE; i++) {
    all_client_microbench_pushes += curr_c_stats[i].microbench_pushes - prev_c_stats[i].microbench_pushes;
    all_client_microbench_pops += curr_c_stats[i].microbench_pops - prev_c_stats[i].microbench_pops;
  }

  memcpy(prev_w_stats, curr_w_stats, WORKERS_PER_MACHINE * (sizeof(struct thread_stats)));
  memcpy(prev_c_stats, curr_c_stats, CLIENTS_PER_MACHINE * (sizeof(struct client_stats)));
  total_throughput = (all_wrkr_completed_reqs) / seconds;
  double zk_WRITE_RATIO = all_wrkr_completed_zk_writes / (double) all_wrkr_completed_reqs;
  double sync_per = all_wrkr_sync_percentage / (double) all_wrkr_completed_reqs;
  double total_treiber_pushes = (all_client_microbench_pushes) / seconds;
  double total_treiber_pops = (all_client_microbench_pops) / seconds;
  double per_s_all_aboard_rmws = (total_all_aboard_rmws) / seconds;

  if (SHOW_STATS_LATENCY_STYLE)
    my_printf(green, "%u %.2f, canc: %.2f, all-aboard: %.2f t_push: %.2f, t_pop: %.2f zk_wr: %.2f, sync_per %.2f\n",
              print_count, total_throughput,
              (total_cancelled_rmws / (double) total_rmws),
              per_s_all_aboard_rmws,
              total_treiber_pushes, total_treiber_pops,
              zk_WRITE_RATIO, sync_per);
  else {
    printf("---------------PRINT %d time elapsed %.2f---------------\n", print_count, seconds / MILLION);
    my_printf(green, "SYSTEM MIOPS: %.2f \n", total_throughput);

    for (int i = 0; i < WORKERS_PER_MACHINE; i++) {
      my_printf(cyan, "T%d: ", i);
      my_printf(yellow, "%.2f MIOPS,  R/S %.2f/s, W/S %.2f/s, QR/S %.2f/s, "
                  "RMWS: %.2f/s, P/S %.2f/s, A/S %.2f/s, C/S %.2f/s ",
                all_stats.cache_hits_per_thread[i],
                all_stats.reads_sent[i],
                all_stats.writes_sent[i],
                all_stats.quorum_reads_per_thread[i],
                all_stats.rmws_completed[i],
                all_stats.proposes_sent[i],
                all_stats.accepts_sent[i],
                all_stats.commits_sent[i]);
      my_printf(yellow, ", BATCHES: Acks %.2f, Ws %.2f, Rs %.2f, R_REPs %.2f",
                all_stats.ack_batch_size[i],
                all_stats.write_batch_size[i],
                all_stats.r_batch_size[i],
                all_stats.r_rep_batch_size[i]);
      my_printf(yellow, ", RtoW %.2f%%, FW %.2f%%",
                100 * all_stats.reads_that_become_writes[i],
                100 * all_stats.failed_rem_write[i]);
      printf("\n");
    }
    printf("\n");
    printf("---------------------------------------\n");
    my_printf(green, "SYSTEM MIOPS: %.2f \n", total_throughput);
  }

}



//assuming microsecond latency -- NOT USED IN ABD

