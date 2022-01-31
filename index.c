//
// Created by david on 8/11/20.
//

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include "index.h"
#include "lib.h"



#ifdef DEBUG2
#define DEBUG
#endif
#ifdef DEBUG
#define LOG
#endif

int currentCommunityId = 0;
int appends_since_last_rebuild = 0;
int max_appends_till_rebuild = 0;

// helper method for index_create, move cs to a new community* array of size $size
community **expandCapacity(community **cs, int old_size, int new_size) {
    community **new = calloc(new_size, sizeof(community*)); // allocate 1 at minimum
    int i;
    for (i = 0; i < old_size; i++)
        new[i] = cs[i];
    free(cs);
    return new;
}

// Read communities given by .nl file into index struct

c_index *index_create(char *filename, graph *g) {
    // setup index
    c_index * ind = malloc(sizeof(c_index));
    ind->n = g->n;
    ind->g = g;
    ind->communities = calloc(g->n, sizeof(community*));
    ind->lengths = calloc(g->n, sizeof(int)); // lengths[i] = number of nonzero community* that ind->communities[i] holds
    ind->list = cl_new();

    int initial_size = 2 * g->e / g->n; // initially allocate 2 * avg degree for each node
    int allocated[g->n]; // allocated[i] = number of community* that ind->communities[i] has malloced for

    int i;
    for (i = 0; i < g->n; i++) {
        ind->communities[i] = calloc(initial_size, sizeof(community*));
        allocated[i] = initial_size;
    }

    // setup file io
    FILE* f = fopen(filename, "r");
    int node; // current node
    int prev_node = -1; // prev node, used to check for sorting
    char ch; // current character

    i = 0; // number of current community
    int buf[g->n]; // buffer holding all nodes that belong to this community so far
    int buf_pos = 0; // current index in buffer

    community* c = malloc(sizeof(community)); // current community

    for (;;) {
        // parse node id
        if (fscanf(f, "%d", &node) == 1) {
            node--; // we need 0 indexed node ids

            if (node < prev_node) {
                printf("file %s is not sorted. exiting...\n", filename);
                exit(1);
            } else {
                prev_node = node;
            }

            buf[buf_pos++] = node; // buffer node

            // expand index array if necessary
            if (ind->lengths[node] == allocated[node]) {
                int new_size = allocated[node] * 2 + 1;
                ind->communities[node] = expandCapacity(ind->communities[node], allocated[node], new_size);
                allocated[node] = new_size;
            }

            // add community to the node's index
            int position = ind->lengths[node]++;
            ind->communities[node][position] = c;
        }

        // on newline, create and store struct for the community we just parsed
        if ((ch = fgetc(f)) == '\n') {
            // set the community's id etc.
            c->id = currentCommunityId++;
            c->n = buf_pos;
            c->nodes = malloc(buf_pos * sizeof(int));
            c->ev = 0;
            c->sibling = NULL;
            c->parent = NULL;

            // copy buffer into community
            int k;
            for (k = 0; k < buf_pos; k++) {
                c->nodes[k] = buf[k];
            }

            // add community to sorted list
            cl_append(ind->list, c);

            // setup for parsing next community
            i++;
            buf_pos = 0;
            prev_node = -1;
            c = malloc(sizeof(community));

        } else if (ch == EOF) {
            break;
        }
    }

    sl_rebuild(ind->list);

#ifdef DEBUG2
    puts("finished parsing communities:");
    cl_print(ind->list);
    sl_print(ind->list->skip_list);
#endif

    return ind;
}

void index_update(c_index *ind, community *a, community *b, community *merged) {
    free(a->nodes);
    free(b->nodes);

    merged->id = currentCommunityId++;

#ifdef DEBUG
    printf("\n--------------- ITEMS: %d %d\n\n", a->id, b->id);
    cl_print(ind->list);
    sl_print(ind->list->skip_list);
#endif

    cl_remove(ind->list, a->id);
    cl_remove(ind->list, b->id);

#ifdef DEBUG
    puts("\nREMOVED\n");
    cl_print(ind->list);
    sl_print(ind->list->skip_list);
#endif

    /*
     * We're just cloning the merged community into a and b. That means it might appear twice in the inverse index's
     * list of communities for a given node.
     *
     * This gives it a higher probability of getting selected in a dice roll in tryMergeRandomPair(),
     * but rolls are cheap, so it probably doesn't matter.
     */
    a->n     = b->n     = merged->n;
    a->ev    = b->ev    = merged->ev;
    a->id    = b->id    = merged->id;
    a->nodes = b->nodes = merged->nodes;

    /*
     * We also run into an issue though. Say we later merge a, and it gets deleted from the community list.
     * b however still exists in the inverse index. Thus it might be randomly selected, even though the community
     * it represents has already been merged. Therefore, merged communities keep a pointer to their clone/sibling
     * community and when they get merged, the sibling is marked dead and won't get randomly selected anymore.
     * (It'll only get rolled, but again rolls are cheap)
     */
    if (a->sibling != NULL)
        a->sibling->parent = a;
    if (b->sibling != NULL)
        b->sibling->parent = b;

    a->sibling = b;
    b->sibling = a;

    // In the list of communities, we just append the merged community once.
    cl_append(ind->list, a);

#ifdef DEBUG
    puts("\nAPPENDED\n");
    cl_print(ind->list);
    sl_print(ind->list->skip_list);
#endif

    // We periodically need to rebuild the skip list because it'll degenerate as we append.
    appends_since_last_rebuild++;
    if (appends_since_last_rebuild > max_appends_till_rebuild) {
        sl_rebuild(ind->list);

#ifdef DEBUG
        printf("passed %d appends since last rebuild. rebuilding:", max_appends_till_rebuild);
        cl_print(ind->list);
        sl_print(ind->list->skip_list);
#endif
    }
}


void index_print(c_index *ind) {
    int i, j;
    puts("printing index:");
    for (i = 0; i < ind->n; i++) {
        printf("\tnode %d of %d:\n", i, ind->n);
        for (j = 0; j < ind->lengths[i]; j++) {
            printf("\t\t");
            printCommunity(ind->communities[i][j]);
        }
    }
}

community_list* cl_new() {
    community_list* cl = malloc(sizeof(community_list));
    cl->n = 0;
    cl->first = NULL;
    cl->last = NULL;
    cl->skip_list = NULL;

    return cl;
}

community_list_item* cl_new_item(community* community) {
    community_list_item* i = malloc(sizeof(community_list_item));
    i->item = community;
    i->next = NULL;
    i->sl_item = NULL;
    return i;
}

void cl_remove(community_list* list, int id) {
    if (id < 0)
        puts("WARNING: ID IS NEGATIVE");

    community_list_item* leftNeighbor = cl_find_nearest_leq(list, id-1);

    if (leftNeighbor == NULL) {
        // Handle first item case
        if (list->first->item->id == id) {
            community_list_item* second = list->first->next;

            if (list->first->sl_item != NULL) {
                // if second is NOT pointed to by a sli, we just give first's sli to second.
                if (second->sl_item != NULL) {
                    second->sl_item = list->first->sl_item;
                    list->first->sl_item->item = second; // if a skiplist item is pointing to this, move it on to the second

                    free(list->first);
                    list->first = second;

                // if we're here, first AND second are pointed to by a sli, which means we need to fix our skiplist
                // I just rebuild it cause im lazy
                } else {
                    free(list->first);
                    list->first = second;

                    sl_rebuild(list);
                }
            }

        } else {
            cl_print(list);
            sl_print(list->skip_list);
            puts("Can't find left neighbor of item to delete.");
            printf("item id = %d\n", id);
            fflush(stdout); // forgot this zzz
            community_list_item* segfault = NULL;
            segfault->sl_item = NULL;
        }

    } else if (leftNeighbor->next->item->id != id) {
        cl_print(list);
        sl_print(list->skip_list);
        puts("Can't find item to delete.");
        printf("item id = %d, left neighbor id = %d, left->next id = %d\n", id, leftNeighbor->item->id, leftNeighbor->next->item->id);
        fflush(stdout);
        community_list_item* segfault = NULL;
        segfault->sl_item = NULL;

    } else {
        community_list_item* rightNeighbor = leftNeighbor->next->next;

        // Fixup last item ptr if needed
        if (leftNeighbor->next == list->last)
            list->last = leftNeighbor;

        // If the element to be deleted is pointed to by a sli, we move the sli's pointer to the left neighbor
        if (leftNeighbor->next->sl_item != NULL) {
#ifdef LOG
            puts("deleting cli that's pointed to by a sli. fixing up sl...");
#endif
            // same as with the first item case, the right neighbor might already have a sl_item which is handled in the else case
            if (leftNeighbor->sl_item == NULL) {
                leftNeighbor->sl_item = leftNeighbor->next->sl_item;
                leftNeighbor->next->sl_item->item = leftNeighbor; // if a skiplist item is pointing to this, move it on to leftNeighbor

                free(leftNeighbor->next);
                leftNeighbor->next = rightNeighbor;

            // if we're here there are two items directly next to each other each pointed to by a sli, so rebuild skiplist
            } else {
                free(leftNeighbor->next);
                leftNeighbor->next = rightNeighbor;

                sl_rebuild(list);
            }

        } else {
            free(leftNeighbor->next);
            leftNeighbor->next = rightNeighbor;
        }

    }

    list->n--;
    if ((int) floor(sqrt(list->n)) != list->skip_list->n) {
        sl_rebuild(list);
    }
}

// O(1) append community. Community id MUST be higher than any other in sl. Caller has to rebuild sl.
void cl_append(community_list* list, community* community) {
    community_list_item* new_item = cl_new_item(community);

    if (list->last == NULL) {
        list->first = list->last = new_item;
        list->skip_list = sl_new(list->first);
    } else {
        list->last->next = new_item;
        list->last = new_item;
        list->skip_list->last->maxid = new_item->item->id;
    }

    list->n++;
}

// Insert a community into list, maintaining sort by id
void cl_insert(community_list* list, community* community) {
    community_list_item* new_item = cl_new_item(community);

    // Handle empty list case
    if (list->first == NULL) {
        list->first = list->last = new_item;
        list->skip_list = sl_new(list->first);

    // Else insert normally.
    } else {
        community_list_item* leftNeighbor = cl_find_nearest_leq(list, community->id);

        // Can only occur if community->id is smaller than everything in the list
        // (or list is empty which is impossible because handled above.)
        if (leftNeighbor == NULL) {
            // Insert as first item
            community_list_item* next = list->first;
            list->first = new_item;
            new_item->next = next;

        } else {
            community_list_item* next = leftNeighbor->next; // next may be NULL
            leftNeighbor->next = new_item;
            new_item->next = next;
        }
    }

    list->n++;

    // If necessary, rebuild skip list
    if ((int) floor(sqrt(list->n)) != list->skip_list->n) {
        sl_rebuild(list);
    }
}

community* cl_linear_find(community_list* list, int id) {
    community_list_item* cur = list->first;
    while (cur != NULL) {
        if (cur->item->id == id)
            return cur->item;
        cur = cur->next;
    }
    return NULL;
}

community* cl_find(community_list* list, int id) {
    community_list_item* i = cl_find_nearest_leq(list, id);
    if (i == NULL) {
        //printf("%d: id %d: got back NULL (should mean even first id %d is smaller)\n", world_rank, id, list->first->item->id);
        return NULL;
    } else if (i->item->id != id) {
        //printf("%d: id %d: only got back leq id %d\n", world_rank, id, i->item->id);
        return NULL;
    } else
        return i->item;
}

// Find community_list_item* with id nearest to and smaller than or equal to param id
community_list_item* cl_find_nearest_leq(community_list* list, int id) {
    skip_list_item* sli = list->skip_list->first;
    int iters = 0;

    while(sli->maxid < id && sli->next != NULL) {
        sli = sli->next;
        iters++;
    }

#ifdef DEBUG
    printf("found appropriate sl item for %d with max id %d after %d iters\n", id, sli->id, iters);
#endif

    community_list_item* cli = sli->item;
    community_list_item* next;
    iters = 0;

#ifdef DEBUG
    printf("begin looking for %d at cli id %d\n", id, cli->item->id);
#endif

    while(cli->next != NULL) {
        iters++;
        next = cli->next;

        // found nearest leq
        if (next->item->id > id)
            break;

        cli = next;
    }

    community_list_item* ret = cli->item->id > id ? NULL : cli;

#ifdef DEBUG
    printf("found leq cl item %d for id %d after %d iters (sli id=%d, ptr=%d). returning %p\n", cli->item->id, id, iters, sli->id, sli->item->item->id, ret);
#endif

    return ret;
}

skip_list_item* sl_new_item(community_list_item* c) {
    skip_list_item* sli = malloc(sizeof(skip_list_item));

    if (c != NULL) {
        sli->item = c;
        sli->maxid = c->item->id;
    }

    sli->next = NULL;

    return sli;
}

skip_list* sl_new(community_list_item* c) {
    skip_list* sl = malloc(sizeof(skip_list));

    sl->n = 1;
    sl->first = sl_new_item(c);
    sl->last = sl->first;

    return sl;
}

// O(n) where n is number of communities in cl
// Grow or shrink skip list. Reuses old skip list to the extent possible
void sl_rebuild(community_list* list) {
    int n = (int) floor(sqrt(list->n));

#ifdef LOG
    printf("rebuilding sl from n=%d to %d for cl n=%d\n", list->skip_list->n, n, list->n);
#endif

#ifdef DEBUG2
    cl_print(list);
#endif

    list->skip_list->n = n;
    max_appends_till_rebuild = 2 * n;
    appends_since_last_rebuild = 0;

    skip_list_item* current_sli = list->skip_list->first;
    skip_list_item* next_sli = current_sli;

    // get rid of old skiplist if exists
    while (current_sli != NULL) {
        next_sli = current_sli->next;
        free(current_sli);
        current_sli = next_sli;
    }

    community_list_item *current_cli = list->first;

    while (current_cli != NULL) {
        current_cli->sl_item = NULL;
        current_cli = current_cli->next;
    }

    current_cli = list->first;
    community_list_item *prev_cli = list->first;

    int i;
    for (i = 0; i < n; i++) {
        int id;

        // Create first or next skip list item
        if (current_sli == NULL) {
            list->skip_list->first = sl_new_item(NULL);
            current_sli = list->skip_list->first;
        } else {
            current_sli->next = sl_new_item(NULL);
            current_sli = current_sli->next;
        }

        // Find this segment's cli and id
        int j;
        for (j = 0; j < n; j++) {
            // next may be NULL at the end?
            if (current_cli->next != NULL) {
                current_cli = current_cli->next;
                id = current_cli->item->id;
            } else {
                id = INT_MAX;
            }
        }

        current_sli->maxid = id;
        current_sli->item = prev_cli;
        prev_cli->sl_item = current_sli;

        prev_cli = current_cli;
    }

    // Add a final skip list item for potential leftover communities from rounding n down
    if (current_cli != list->last) {
        current_sli->next = sl_new_item(NULL);
        current_sli = current_sli->next;
        current_sli->maxid = list->last->item->id;
        current_sli->item = current_cli;
        current_cli->sl_item = current_sli;
    }

    list->skip_list->last = current_sli;

#ifdef LOG
    cl_benchmark(list, 1000);
#endif

#ifdef DEBUG
    sl_print(list->skip_list);
#endif

    cl_check_integrity(list);
}

void sl_print(skip_list * sl) {
    skip_list_item* current = sl->first;
    int i = 0;
    puts("sl:");
    while(current != NULL) {
        printf("\t%d: maxid %d points to id %d\n", i++, current->maxid, current->item->item->id);
        current = current->next;
    }
}

void cl_print(community_list* cl) {
    community_list_item* current = cl->first;
    int i = 0;
    puts("cl:");
    printf("\tfirst: %d\n", cl->first->item->id);
    printf("\tlast: %d\n", cl->last->item->id);
    puts("\titems:");
    while (current != NULL) {
        printf("\t\t%d: id %d", i++, current->item->id);

        if (current->sl_item != NULL) {
            printf(" (sli: maxid %d points to %d)", current->sl_item->maxid, current->sl_item->item->item->id);
        }

        puts("");
        printCommunity(current->item);
        current = current->next;
    }
}

void index_print_meta(c_index* ind) {
    int n = 1000;
    cl_benchmark(ind->list, n);
}

// Return number of ns for finding n random elements
void cl_benchmark(community_list* cl, int n) {
    struct timespec tstart={0,0}, tend={0,0};

    puts("benching...");

    // Bench skip list find
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    int i;
    for (i = 0; i < n; i++) {
        int find = randInt(0, cl->n);
        cl_find(cl, find);
    }
    clock_gettime(CLOCK_MONOTONIC, &tend);
    double seconds = ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
    printf("\tfinding %d items took about %.5f seconds\n", n, seconds);

    // Bench linear find (commented out cuz takes too long)
    //clock_gettime(CLOCK_MONOTONIC, &tstart);
    //for (i = 0; i < n; i++) {
    //    int find = randInt(0, cl->n);
    //    cl_linear_find(cl, find);
    //}
    //clock_gettime(CLOCK_MONOTONIC, &tend);
    //seconds = ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec);
    //printf("\tlinear finding %d items took about %.5f seconds\n", n, seconds);

    int count = 0;

    community_list_item* cli = cl->first;
    while (cli != NULL) {
        cli = cli->next;
        count++;
    }

    printf("\tcounted %d cli(n=%d)\n", count, cl->n);

    count = 0;
    skip_list_item* sli = cl->skip_list->first;
    while (sli != NULL) {
        sli = sli->next;
        count++;
    }

    printf("\tcounted %d sli(n=%d)\n", count, cl->skip_list->n);
}

void cl_check_integrity(community_list* cl) {
    community_list_item* current = cl->first;
    int lastid = -1;

    while(current != NULL) {
        if (current->sl_item != NULL) {
            if (current->sl_item->item != current) {
                printf("ERROR: cli %d is pointing to sli %d\n", current->item->id, current->sl_item->maxid);
            }
        }

        if (current->item->id < lastid) {
            printf("ERROR: sorted order is broken at cli %d, last is %d\n", current->item->id, lastid);
        } else {
            lastid = current->item->id;
        }

        current = current->next;
    }

    skip_list_item* prev_sli = cl->skip_list->first;
    skip_list_item* current_sli = prev_sli;
    while (current_sli != NULL) {
        if (current_sli->item->item->id > current_sli->maxid) {
            printf("ERROR: sli maxid=%d points to cli id=%d\n", current_sli->maxid, current_sli->item->item->id);
        }

        if (current_sli->maxid <= prev_sli->maxid && current_sli != prev_sli) {
            printf("ERROR: maxid sorted order broken at sli %d (prev: %d)\n", current_sli->maxid, prev_sli->maxid);
        }

        if (current_sli->item->item->id <= prev_sli->item->item->id && current_sli != prev_sli) {
            printf("ERROR: cli pointer sorted order broken at sli %d (prev: %d)\n", current_sli->item->item->id, prev_sli->item->item->id);
        }

        prev_sli = current_sli;
        current_sli = current_sli->next;
    }

    fflush(stdout);
}
