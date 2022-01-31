#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <mpi.h>
#include <execinfo.h>
#include <unistd.h>
#include "graph.h"
#include "index.h"
#include "main.h"
#include "lib.h"

#define TAG_TERMINATE 420
#define TAG_UPDATE 69

#define PRINT_RESULTS 0

double minNodeOverlapPerc;
double minDisjointEdgesPerc;
double minEvDelta;
int maxn = 400; // do not try to merge communities larger than this


// Profiling
int npairs = 0; // number of pairs checked
int nnodes = 0; // number of pairs passing node overlap
int nedges = 0; // number of pairs passing edge overlap
int nevs = 0;   // number of pairs passing ev improvement

int nsent_updates = 0;
int nreceived_updates = 0;
int ninvalid_updates = 0;
int nstale_updates = 0;
int nmerged_updates = 0;

int max_update_time = 0;
int min_update_time = 999999;

int pairs_since_success = 0;

int world_rank;

c_index *ind;

time_t start_time;

void sigsegv_handler(int sig) {
  void *array[20];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 20);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

void sigintHandler(int sig_num) {
  community_list_item* last = ind->list->last;
  if (world_rank == 0)
    printf("%d: at %d, received %d, invalid %d, stale %d, merged %d\n", world_rank, last->item->id, nreceived_updates, ninvalid_updates, nstale_updates, nmerged_updates);
  //cl_print(ind->list);
  else
    printf("%d: at %d, sent %d, recvd %d\n", world_rank, last->item->id, nsent_updates, nreceived_updates);

  fflush(stdout);

  MPI_Finalize();

  exit(15);
}

void printDebug(char *format_string, ...) {
#ifdef DEBUG
  va_list ap;
  va_start(ap, format_string);
  vprintf(format_string, ap);
#endif
}

c_index *prepare(char *graphFile, char *communitiesFile) {
  FILE *fb = fopen(graphFile, "r");
  graph *g = fromMetis(fb);
  printDebug("n: %d, e: %d\n", g->n, g->e);
  printDebug("last edge: %d\n", g->edgelist[g->e - 1]);
  puts(graphFile);

  ind = index_create(communitiesFile, g);

  return ind;
}

void setParams(double pminNodeOverlapPerc, double pminDisjointEdgesPerc, double pminEvDelta) {
  minNodeOverlapPerc = pminNodeOverlapPerc;
  minDisjointEdgesPerc = pminDisjointEdgesPerc;
  minEvDelta = pminEvDelta;
}

int tries = 0;

merge_result tryMergeRandomPair(c_index *ind) {
  community *c1;
  community *c2;

  int node;

  merge_result ret;
  ret.id1 = -1;
  ret.id2 = -1;

  // Find a random node that is in at least two communities
  // and select two random and distinct communities from those
  do {
    node = randInt(0, ind->n);

    if (ind->lengths[node] < 2)
      continue;

    int c1index = randInt(0, ind->lengths[node]);
    int c2index = randInt(0, ind->lengths[node]);
    c1 = ind->communities[node][c1index];
    c2 = ind->communities[node][c2index];

    //if (c1->id != c2->id)
    //    printDebug("Comparing @ node %5d: %5d v %5d", node, c1index, c2index);

  } while (ind->lengths[node] < 2 || c1->id == c2->id || c1->n > maxn || c2->n > maxn);

  while (c1->parent != NULL)
    c1 = c1->parent;
  while (c2->parent != NULL)
    c2 = c2->parent;

  pairs_since_success++;
  tries++;
  community *result = checkPair(ind->g, c1, c2);

  if (result) {
    if (PRINT_RESULTS) {
      time_t t = time(NULL) - start_time;
      int id1 = c1->id;
      int id2 = c2->id;
      float ev1 = c1->ev;
      float ev2 = c2->ev;
      int n1 = c1->n;
      int n2 = c2->n;
      //index_update(ind, c1, c2, result);
      char* embedded = (result->n == n1 || result->n == n2) ? "(embedded)" : "";
      printf("pairs %10d | node pass %10d | edge pass %10d | time %5d | id1 %8d | id2 %8d | newid %8d | evs: %1.5f (%2d) / %1.5f (%2d) -> %1.5f (%2d) %s\n",
          pairs_since_success, nnodes, nedges, t, id1, id2, result->id, ev1, n1, ev2, n2, result->ev, result->n, embedded);
      pairs_since_success = 0;
      nnodes = 0;
      nedges = 0;
    }

    ret.id1 = c1->id;
    ret.id2 = c2->id;
  }

#ifdef DEBUG
  communityIsMessedUp(c1);
  communityIsMessedUp(c2);
#endif

  free(result);

  return ret;
}

// Returns pointer to merged community if merge makes sense, else 0
community *checkPair(graph *g, community *c1, community *c2) {
  community *ret = 0;
  npairs++;

  // Compute the number of overlapping nodes
  int commonNodes = commonElements(c1, c2);
  int largerCommunitySize = c1->n > c2->n ? c1->n : c2->n;
  int minOverlappingNodes = largerCommunitySize * minNodeOverlapPerc;
  printDebug("\t%5d common nodes, larger has %5d => cutoff = %5d", commonNodes, largerCommunitySize, minOverlappingNodes);

  // If that passes a threshold:
  if (commonNodes > minOverlappingNodes) {
    nnodes++;

    community *a = setMinus(c1, c2); // A is part of c1 that doesn't overlap with c2
    community *c = setMinus(c2, c1); // C is part of c2 that doesn't overlap with c1
    community *larger = a->n > c->n ? a : c;

    int disjointEdges = edgesBetweenSubsets(g, a, c);
    int innerEdges = edgesBetweenSubsets(g, larger, larger) / 2; // divide by two because this counts each edge twice

    printDebug(" PASS\t inner edges: %6d disjoint edges: %6d", innerEdges, disjointEdges);

    // If there are enough edges between the disjoint parts of c1 and c2:
    if (disjointEdges > minDisjointEdgesPerc * innerEdges) {
      nedges++;

      community *merged = merge(c1, c2);
      larger = c1->n > c2->n ? c1 : c2;

      // Find the second smallest eigenvalues
      double mergedEv = communityEv(merged, g);
      double largerEv = communityEv(larger, g);

      printDebug(" PASS mergedEv: %1.5f largerEv: %1.5f", mergedEv, largerEv);

      // If the eigenvalue improves
      if (mergedEv - minEvDelta > largerEv) {
        nevs++;

        printDebug(" PASS!");
        ret = merged;

      } else { // throw away
        free(merged->nodes);
        free(merged);
      }
    }

    free(a->nodes);
    free(a);
    free(c->nodes);
    free(c);

  } else if (commonNodes == c1->n || commonNodes == c2->n) { // iff one community is embedded in the other,
    // we say the merge makes sense, since (TODO) we don't want embedded communities I guess
    ret = merge(c1, c2);
    ret->ev = commonNodes == c1->n ? c2->ev : c1->ev; // also set the ev
  }

  printDebug("\n");

  return ret;
}

int main(int argc, char** argv) {
  setParams(0.1, 0.5, 0.001);

  if (argc != 4) {
    printf("usage: %s metis_graph communities_file timeout_seconds\n", argv[0]);
    puts("IMPORTANT: remember to preprocess the input files using `preprocess.py file > newfile` (handles both .metis and .nl)");
  }

  signal(SIGINT, sigintHandler);
  signal(SIGTERM, sigintHandler);
  signal(SIGVTALRM, sigintHandler);

  signal(SIGSEGV, sigsegv_handler);


  reset_clock();

  // Forms communicator, creates all MPI variables, etc...
  // Args aren't necessary
  MPI_Init(NULL, NULL);

  int world_size;
  // Get size of communicator (= number of processes assigned)
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);


  // Get rank (= contiguous ID starting at 0) of *this* process
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);


  // Pray to rngsus
  srand(time(NULL) * world_rank);

  char processor_name[MPI_MAX_PROCESSOR_NAME];
  int name_len;
  MPI_Get_processor_name(processor_name, &name_len);

  printf("hello from %d on %s\n", world_rank, processor_name);

  char *graphFile = argv[1];
  char *communitiesFile = argv[2];

  c_index *ind = prepare(graphFile, communitiesFile);

  community* c1;
  community* c2;
  community* merged;

  // Synchronize
  MPI_Barrier(MPI_COMM_WORLD);

  // Set timeout
  struct itimerval timer;
  timer.it_value.tv_sec = atoi(argv[3]);
  timer.it_value.tv_usec = 0;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  //setitimer (ITIMER_VIRTUAL, &timer, 0);
  unsigned long stime = (unsigned long) time(NULL);


  ////////////
  // MASTER //
  ////////////

  if (world_rank == 0) {
    int update_ids[2];

    MPI_Status status;

    MPI_Request requests[world_size];

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (;;) {

      MPI_Recv(
          &update_ids,
          2,
          MPI_INT,
          MPI_ANY_SOURCE,
          TAG_UPDATE,
          MPI_COMM_WORLD,
          &status
          );

      nreceived_updates++;

      // IDK how but this happens sometimes...
      if (update_ids[0] == -1 || update_ids[1] == -1) {
        ninvalid_updates++;
        continue;
      }

      // TODO: Optimization. Don't keep a full community list. Just an int array which tracks merges.
      c1 = cl_find(ind->list, update_ids[0]);
      c2 = cl_find(ind->list, update_ids[1]);

      //printf("RECV update %d %d (from: %d)\n", update_ids[0], update_ids[1], status.MPI_SOURCE);

      if (c1 == NULL || c2 == NULL) { // if c1 or c2 have been merged in the meanwhile, ignore the update
        nstale_updates++;
        continue;
      }

      // Send id of merged pair to all.
      // TODO: Optimization potential, use broadcasting algorithm.
      int i;
      for (i = 1; i < world_size; i++) {
        MPI_Send(
            &update_ids,
            2,
            MPI_INT,
            i,
            TAG_UPDATE,
            MPI_COMM_WORLD//,
            //&requests[i]
            );
      }

      merged = merge(c1, c2);
      index_update(ind, c1, c2, merged);
      nmerged_updates++;
      //if (nmerged_updates % 1000 == 0) {
      //        printf("merged %d\n", nmerged_updates);
      //}

      if ((unsigned long) time(NULL) - stime > atoi(argv[3])) {
        break;
      }



      //printf("SENDALL update %d %d -> %d (at: %u)\n", update_ids[0], update_ids[1], c1->id, (unsigned long) time(NULL));
      //fflush(stdout);
    }
#pragma clang diagnostic pop

    // Send terminate msges
    int i;
    for (i = 1; i < world_size; i++) {
      MPI_Isend(
          &update_ids,
          2,
          MPI_INT,
          i,
          TAG_TERMINATE,
          MPI_COMM_WORLD,
          &requests[i]
          );
    }

  ////////////
  // SLAVE  //
  ////////////

  } else {
    int found_update_ids[2] = {-1, -1};
    int recvd_update_ids[2];

    int received_message;
    MPI_Request receive_request;
    MPI_Status receive_status;
    int message_waiting = 0;

    int sent_message;
    MPI_Request send_request = NULL;
    MPI_Status send_status;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for(;;) {
      MPI_Iprobe(
          MPI_ANY_SOURCE,
          MPI_ANY_TAG,
          MPI_COMM_WORLD,
          &message_waiting,
          &receive_status
          );

      while (message_waiting) {
        //if (found_update_ids[0] != -1) {
        //    printf("DROP update %d %d (rank: %d)\n", found_update_ids[0], found_update_ids[1], world_rank);
        //    found_update_ids[0] = -1; // TODO not absolutely necessary
        //}

        //printf("%d:%s\t receiving a message\n", world_rank, processor_name);

        MPI_Recv(
            &recvd_update_ids,
            2,
            MPI_INT,
            0,
            MPI_ANY_TAG,
            MPI_COMM_WORLD,
            &receive_status
            );

        switch (receive_status.MPI_TAG) {
          case TAG_UPDATE:
            c1 = cl_find(ind->list, recvd_update_ids[0]);
            c2 = cl_find(ind->list, recvd_update_ids[1]);

            if (c1 == NULL) {
              printf("%d about to die: got NULL for id %d\n", world_rank, recvd_update_ids[0]);
              community_list_item* last = ind->list->last;
              printf("stuck on %d\n", last->item->id);
              fflush(stdout);
            }

            if (c2 == NULL) {
              printf("%d about to die: got NULL for id %d\n", world_rank, recvd_update_ids[1]);
              community_list_item* last = ind->list->last;
              printf("stuck on %d\n", last->item->id);
              fflush(stdout);
            }

            merged = merge(c1, c2);
            nreceived_updates++;

            index_update(ind, c1, c2, merged);

            //printf("RECV update %d %d -> %d (rank: %d) (tries since last merge: %d)\n", recvd_update_ids[0], recvd_update_ids[1], c1->id, world_rank, tries);
            //fflush(stdout);
            tries = 0;

            break;

          case TAG_TERMINATE:
            goto exit;
        }

        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &message_waiting, &receive_status);
        //printf("%d got waiting: %d\n", world_rank, message_waiting);
      }

      merge_result result = tryMergeRandomPair(ind);
      found_update_ids[0] = result.id1;
      found_update_ids[1] = result.id2;

      //printf("%d:%s\t update is %d/%d\n", world_rank, processor_name, result.id1, result.id2);

      if (found_update_ids[0] != -1) {
        nsent_updates++;
        unsigned long long mergetime = (unsigned long) time(NULL) - stime;
        if (mergetime > max_update_time) {
          max_update_time = mergetime;
        } else if (mergetime < min_update_time) {
          min_update_time = mergetime;
        }
        stime = (unsigned long) time(NULL);
        //printf("SEND update %d %d (rank: %d at %d now)\n", found_update_ids[0], found_update_ids[1], world_rank, ind->list->last->item->id);
        //fflush(stdout);

        //printf("%d:%s\t now sending update %d/%d\n", world_rank, processor_name, found_update_ids[0], found_update_ids[1]);
        MPI_Isend(
            &found_update_ids,
            2,
            MPI_INT,
            0,
            TAG_UPDATE,
            MPI_COMM_WORLD,
            &send_request
            );

        found_update_ids[0] = -1;
      }

      //MPI_Irecv(
      //        &recvd_update_ids,
      //        2,
      //        MPI_INT,
      //        0,
      //        MPI_ANY_TAG,
      //        MPI_COMM_WORLD,
      //        &receive_request
      //);

      //test:
      //MPI_Test(&receive_request, &received_message, &receive_status);

      //if (send_request != NULL)
      //    MPI_Test(&send_request, &sent_message, &send_status);

      //if (received_message) {
      //    switch (receive_status.MPI_TAG) {
      //        case TAG_UPDATE:
      //            c1 = cl_find(ind->list, recvd_update_ids[0]);
      //            c2 = cl_find(ind->list, recvd_update_ids[1]);
      //            merged = merge(c1, c2);
      //            index_update(ind, c1, c2, merged);
      //            printf("RECV update %d %d -> %d (rank: %d)\n", recvd_update_ids[0], recvd_update_ids[1], c1->id, world_rank);
      //            if (found_update_ids[0] != -1) {
      //                printf("DROP update %d %d (rank: %d)\n", found_update_ids[0], found_update_ids[1], world_rank);
      //                found_update_ids[0] = -1;
      //            }
      //            continue;
      //        case TAG_TERMINATE:
      //            goto exit;
      //    }

      //} else if (found_update_ids[0] != -1) {
      //    printf("SEND update %d %d (rank: %d)\n", found_update_ids[0], found_update_ids[1], world_rank);

      //    MPI_Isend(
      //            &found_update_ids,
      //            2,
      //            MPI_INT,
      //            0,
      //            TAG_UPDATE,
      //            MPI_COMM_WORLD,
      //            &send_request
      //    );

      //    found_update_ids[0] = -1;

      //    goto test;

      //} else {
      //    // Performance TODOs:
      //    // * also send eigenvalue of result
      //    // * try multiple merges in between checking for new messages, or just try till you find sth, chance of
      //    //   having to throw the result away is probably abysmal
      //    merge_result result = tryMergeRandomPair(ind);
      //    found_update_ids[0] = result.id1;
      //    found_update_ids[1] = result.id2;

      //    goto test;
      //}
    }
#pragma clang diagnostic pop

  }

  // Stop.
exit:;
     community_list_item* last = ind->list->last;
     if (world_rank == 0) {
       printf("%d@%s: at %d, received %d, invalid %d, stale %d, merged %d\n", world_rank, processor_name, last->item->id, nreceived_updates, ninvalid_updates, nstale_updates, nmerged_updates);

       stime = (unsigned long) time(NULL);
       while ((unsigned long) time(NULL) - stime < 600);
       cl_print(ind->list);
     } else {
       //printf("%d@%s: at %d, sent %d, recvd %d, min %d, max %d\n", world_rank, processor_name, last->item->id, nsent_updates, nreceived_updates, min_update_time, max_update_time);
     }

     fflush(stdout);
     MPI_Finalize();

     //community_list_item* current = ind->list->first;
     //int prev = -1;
     //while (current != NULL) {
     //    if (current->item->id != prev + 1)
     //        printf("%d %d\n", world_rank, current->item->id);
     //    prev = current->item->id;
     //    current = current->next;
     //}

     // TODO have master print final data

     return 0;
}
