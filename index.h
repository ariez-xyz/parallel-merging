//
// Created by david on 8/11/20.
//

#ifndef MPICOMM_INDEX_H
#define MPICOMM_INDEX_H
#include "graph.h"

typedef struct community_list_item {
    struct community_list_item* next;
    struct skip_list_item* sl_item;
    community* item;
} community_list_item;

typedef struct community_list {
    struct community_list_item* first;
    struct community_list_item* last;
    struct skip_list* skip_list;
    int n;
} community_list;

typedef struct skip_list_item {
    struct skip_list_item* next;
    int maxid;
    community_list_item* item;
} skip_list_item;

typedef struct skip_list {
    struct skip_list_item* first;
    struct skip_list_item* last;
    int n;
} skip_list;

typedef struct {
    graph *g;
    int n; // length of lengths[] and communities[]
    int *lengths; // lengths[i] = number of community* in communities[i]

    community ***communities; // array of arrays of community*'s
    // where communities[i] is an array of community* containing node i
    // and communities[i][j] is the j-th community* containing node i
    // and len(communities[i]) = lengths[i]

    community_list* list; // Linked list of community* sorted by id
} c_index;

// in main.c
extern int world_rank;

community_list* cl_new();

c_index *index_create(char *filename, graph *g);

void index_print(c_index *ind);

void index_update(c_index *ind, community *a, community *b, community *merged);

void cl_remove(community_list* list, int id);

void cl_append(community_list* list, community* community);

void cl_insert(community_list* list, community* community);

community* cl_linear_find(community_list* list, int id);

community* cl_find(community_list* list, int id);

community_list_item* cl_find_nearest_leq(community_list* list, int id);

void cl_print(community_list* cl);

skip_list* sl_new(community_list_item* c);

skip_list_item* sl_new_item(community_list_item* c);

void sl_rebuild(community_list* list);

void sl_print(skip_list * sl);

void index_print_meta();

void cl_benchmark(community_list* cl, int n);

void cl_check_integrity(community_list* cl);

#endif //MPICOMM_INDEX_H
