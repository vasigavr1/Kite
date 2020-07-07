cmake_minimum_required(VERSION 3.18)
project(odyssey)

set(odyssey_VERSION_MAJOR 1)

include_directories(odyssey-shared/include/libhrd include/kite odyssey-shared/include/mica odyssey-shared/include/general_util zookeeper/include/zookeeper
        include/kite_inline_util /usr/include/)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -std=gnu1x -Wno-address-of-packed-member \
-Wno-format-truncation -g  -march=native")




set(COMMON_FILES
        odyssey-shared/src/mica/kvs.c
        #libhrd
        odyssey-shared/include/libhrd/hrd.h
        odyssey-shared/src/libhrd/hrd_util.c
        odyssey-shared/src/libhrd/hrd_conn.c
        #mica
        odyssey-shared/src/mica/city.c
        odyssey-shared/include/mica/kvs.h
        odyssey-shared/include/mica/city.h
        /usr/include/infiniband/verbs.h
        odyssey-shared/src/client/client.c
        odyssey-shared/include/general_util/generic_macros.h
        odyssey-shared/include/general_util/client_if_util.h
        odyssey-shared/include/general_util/interface.h
        odyssey-shared/include/general_util/latency_util.h
        odyssey-shared/include/general_util/stats.h
        odyssey-shared/include/general_util/top.h
        odyssey-shared/include/general_util/init_func.h
        odyssey-shared/include/general_util/generic_inline_util.h
        odyssey-shared/include/general_util/init_connect.h
        odyssey-shared/include/general_util/generic_opcodes.h
        odyssey-shared/include/general_util/trace_util.h
        odyssey-shared/include/general_util/rdma_gen_util.h
        odyssey-shared/include/general_util/debug_util.h
        odyssey-shared/include/general_util/inline_util.h
        odyssey-shared/include/general_util/sizes.h
        odyssey-shared/include/general_util/config_util.h)

set(KITE_SOURCE_FILES
        src/kite/util.c
        src/kite/main.c
        src/kite/worker.c
        src/kite/stats.c
        include/kite_inline_util/kite_inline_util.h
        include/kite/main.h
        include/kite/util.h
        include/kite/config.h
        include/kite/opcodes.h
        include/kite_inline_util/generic_util.h
        include/kite_inline_util/kvs_util.h
        include/kite_inline_util/kite_debug_util.h
        include/kite_inline_util/kite_config_util.h
        include/kite_inline_util/paxos_util.h
        include/kite_inline_util/reserve_stations_util.h
        include/kite_inline_util/communication_utility.h
        include/kite_inline_util/paxos_generic_util.h
        include/kite/messages.h
        include/kite/buffer_sizes.h
        )





add_executable(kite ${KITE_SOURCE_FILES} ${COMMON_FILES})



set_target_properties(kite PROPERTIES COMPILE_FLAGS -DKITE)


target_link_libraries(kite pthread ibverbs rt numa rdmacm)
