cmake_minimum_required(VERSION 3.18)
project(odyssey)

set(odyssey_VERSION_MAJOR 1)

include_directories(od-lib/include/libhrd include/kite od-lib/include/mica od-lib/include/general_util zookeeper/include/zookeeper
        include/kite_inline_util /usr/include/)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -std=gnu1x -Wno-address-of-packed-member \
-Wno-format-truncation -g  -march=native")




set(COMMON_FILES
        od-lib/src/mica/kvs.c
        #libhrd
        od-lib/include/libhrd/hrd.h
        od-lib/src/libhrd/hrd_util.c
        od-lib/src/libhrd/hrd_conn.c
        #mica
        od-lib/src/mica/city.c
        od-lib/include/mica/kvs.h
        od-lib/include/mica/city.h
        /usr/include/infiniband/verbs.h
        od-lib/src/client/client.c
        od-lib/include/general_util/generic_macros.h
        od-lib/include/general_util/client_if_util.h
        od-lib/include/general_util/interface.h
        od-lib/include/general_util/latency_util.h
        od-lib/include/general_util/stats.h
        od-lib/include/general_util/top.h
        od-lib/include/general_util/init_func.h
        od-lib/include/general_util/generic_inline_util.h
        od-lib/include/general_util/init_connect.h
        od-lib/include/general_util/generic_opcodes.h
        od-lib/include/general_util/trace_util.h
        od-lib/include/general_util/rdma_gen_util.h
        od-lib/include/general_util/debug_util.h
        od-lib/include/general_util/inline_util.h
        od-lib/include/general_util/sizes.h
        od-lib/include/general_util/config_util.h)

set(KITE_SOURCE_FILES
        src/kite/util.c
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
