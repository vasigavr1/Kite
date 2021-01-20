# Note:
This is a submodule part of the [Odyssey](https://github.com/vasigavr1/Odyssey) project. Please refer to the odyssey project for execution scripts. CMakeLists.txt is not maintained. Please use the CMakeLists.txt found in vasigavr1/odyssey to compile.

# Kite
Kite is a replicated, RDMA-enabled Key-Value Store that enforces available Release Consistency.
Kite implements a read/write/RMW API an uses:
1. Eventual Store for relaxed reads  & writes
2. MW-ABD (simply called ABD) for releases & acquires (i.e. linearizable reads/writes)
3. Basic Paxos for RMWs.

## Eventual Store (ES)
Eventual Store is a protocol that provides per-keu Sequential consistency.
ES implements reads locally and incurs a broadcast round for writes.


## ABD Algorithm (Attiya, Bar-Noy, Dolev )

ABD is an algorithm that implements linearizable reads and writes
Writes incur 2 broadcast rounds and reads incur 1 broadcast round with an conditional second round.

## Paxos
Paxos is implemented as such:
* Basic Paxos (leaderless with 3 rtts)
* Key-granularity: Paxos commands to different keys do not interact
* With both release and acquire semantics
* Compare And Swaps can be weak: they can fail locally if the comparison fails locally


## Kite API
Kite API contains two flavours (a blocking and a nonblocking) of the following commands
1. read()
2. write()
3. release()
4. acquire()
5. CAS_strong()
6. CAS_weak()
7. FAA()

The Kite API can be used by the client threads.
./src/client.c already contains implementations of
* The Treiber Stack
* Michael & Scott Queues
* Harris and Michael Lists
* A circular producer consumer pattern

Also a User Interface to issue requests from the Command Line is available.


## Optimizations
The protocol is implemented over UD Sends and Receives.
All messages are batched.

--------------------------------------------------------------
The title of the project is inspired by this
https://www.youtube.com/watch?v=NQBFCaQPtEs