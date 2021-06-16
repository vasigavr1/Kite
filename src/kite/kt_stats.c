#include "kt_util.h"

void print_latency_stats(void);

void kite_stats(stats_ctx_t *ctx) {

  double seconds = ctx->seconds;
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
  uint64_t total_cancelled_rmws = 0, total_rmws = 0, total_all_aboard_rmws = 0;

  get_all_wrkr_stats(ctx, WORKERS_PER_MACHINE, sizeof(t_stats_t));
  for (int i = 0; i < WORKERS_PER_MACHINE; i++) {
    all_wrkr_completed_zk_writes += (curr_w_stats[i].rmws_completed + curr_w_stats[i].releases_per_thread +
                                     curr_w_stats[i].writes_per_thread -
                                     (prev_w_stats[i].rmws_completed + prev_w_stats[i].releases_per_thread +
                                      prev_w_stats[i].writes_per_thread));

    all_wrkr_sync_percentage += (curr_w_stats[i].rmws_completed + curr_w_stats[i].releases_per_thread +
                                 curr_w_stats[i].acquires_per_thread -
                                 (prev_w_stats[i].rmws_completed + prev_w_stats[i].releases_per_thread +
                                  prev_w_stats[i].acquires_per_thread));
  }

  uint64_t all_client_microbench_pushes = 0, all_client_microbench_pops = 0;
  for (int i = 0; i < CLIENTS_PER_MACHINE; i++) {
    all_client_microbench_pushes += curr_c_stats[i].microbench_pushes - prev_c_stats[i].microbench_pushes;
    all_client_microbench_pops += curr_c_stats[i].microbench_pops - prev_c_stats[i].microbench_pops;
  }

  memcpy(prev_w_stats, curr_w_stats, WORKERS_PER_MACHINE * (sizeof(struct thread_stats)));
  memcpy(prev_c_stats, curr_c_stats, CLIENTS_PER_MACHINE * (sizeof(struct client_stats)));
  //total_throughput = (all_wrkr_completed_reqs) / seconds;
  //double zk_WRITE_RATIO = all_wrkr_completed_zk_writes / (double) all_wrkr_completed_reqs;
  //double sync_per = all_wrkr_sync_percentage / (double) all_wrkr_completed_reqs;
  //double total_treiber_pushes = (all_client_microbench_pushes) / seconds;
  //double total_treiber_pops = (all_client_microbench_pops) / seconds;
  //double per_s_all_aboard_rmws = (total_all_aboard_rmws) / seconds;
  //
  uint64_t total_reqs = ctx->all_aggreg->total_reqs;
  if (SHOW_AGGREGATE_STATS) {
    t_stats_t *all_aggreg = ctx->all_aggreg;

    my_printf(green, "%u %.2f, all-aboard: %.2f, canc: %.2f,  \n",
              ctx->print_count, per_sec(ctx, total_reqs),
              per_sec(ctx, all_aggreg->all_aboard_rmws),
              (double) all_aggreg->cancelled_rmws /
              (double) all_aggreg->rmws_completed);

  }
  else {
    t_stats_t *all_per_t = ctx->all_per_t;


    printf("---------------PRINT %d time elapsed %.2f---------------\n",
           ctx->print_count, ctx->seconds);
    my_printf(green, "SYSTEM MIOPS: %.2f \n",
              per_sec(ctx, total_reqs));

    for (int i = 0; i < WORKERS_PER_MACHINE; i++) {
      my_printf(cyan, "T%d: ", i);
      my_printf(yellow, "%.2f MIOPS,  "
                        "R/S %.2f/s, W/S %.2f/s, " //QR/S %.2f/s, "
                        "RMWS: %.2f/s, AB:%.2f/s, P/S %.2f/s, A/S %.2f/s, C/S %.2f/s ",
                per_sec(ctx, all_per_t[i].total_reqs),
                per_sec(ctx, all_per_t[i].reads_sent),
                per_sec(ctx, all_per_t[i].writes_sent),
                //per_sec(ctx, all_per_t[i].quorum_reads_per_thread),
                per_sec(ctx, all_per_t[i].rmws_completed),
                per_sec(ctx, all_per_t[i].all_aboard_rmws),
                per_sec(ctx, all_per_t[i].proposes_sent),
                per_sec(ctx, all_per_t[i].accepts_sent),
                per_sec(ctx, all_per_t[i].commits_sent));
      //my_printf(yellow, ", BATCHES: Acks %.2f, Ws %.2f, Rs %.2f, R_REPs %.2f",
      //          all_stats.ack_batch_size[i],
      //          all_stats.write_batch_size[i],
      //          all_stats.r_batch_size[i],
      //          all_stats.r_rep_batch_size[i]);
      printf("\n");
    }
    printf("\n");
    printf("---------------------------------------\n");
    my_printf(green, "SYSTEM MIOPS: %.2f \n", per_sec(ctx, total_reqs));
  }
  memset(ctx->all_aggreg, 0, sizeof(t_stats_t));
}



//assuming microsecond latency -- NOT USED IN ABD

