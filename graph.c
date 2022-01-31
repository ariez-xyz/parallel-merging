//
// Created by ariez on 4/10/20.
//

#include "graph.h"
#include "lib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

// just for debugging
void communityIsMessedUp(community *a) {
#ifdef DEBUG
    int i;
    int nzeroes = 0;
    for (i = 0; i < a->n; i++) {
        if (a->nodes[i] == 0) {
            nzeroes++;
        } else if (a->nodes[i] > 10000000) {
            puts("Huge node id");
        }
    }
    if (nzeroes > 1) {
        puts("node 0 should not appear more than once");
    }
#endif
}

// malloc n, 2e
// read edges sequentially into edgelist
// keep count of current index
// put current index in nodemap on newline
// unless currently on last node
// assumes edges are sorted on a per-node basis
graph *fromMetis(FILE *f) {
    graph *g = malloc(sizeof(graph));

    fscanf(f, "%d", &g->n);
    fscanf(f, "%d", &g->e);

    g->e *= 2;

    g->nodemap = malloc(sizeof(int) * (g->n + 1)); // allocate extra slot for the end of the last node's edgelist
    g->edgelist = malloc(sizeof(int) * g->e);      // this simplifies iterating

    int ch;
    int current_node = -1; // begin with node id -1: we will be reading the header's newline and adding 1 as a consequence
    int current_edge = 0;
    int prev_edge = -1; // previous edge for keeping track of whether file is sorted

    while ((ch = fgetc(f)) != EOF) {
        if (ch == (int) '\n') {
            current_node++;
            prev_edge = -1;

            // break if all nodes have been read
            if (current_node >= g->n)
                break;

            g->nodemap[current_node] = current_edge;
        }

        if (fscanf(f, "%d", &g->edgelist[current_edge])) {
            g->edgelist[current_edge]--; // metis files are 1-indexed
            current_edge++;

            if (g->edgelist[current_edge] < prev_edge) {
                printf("metis graph is not sorted. exiting...\n");
                exit(1);
            } else {
                prev_edge = g->edgelist[current_edge];
            }
        }
    }

    g->nodemap[g->n] = g->e;

    return g;
}

// true iff edgelist[nodemap[u] : nodemap[u + 1]] has v
int hasEdge(graph *g, int u, int v) {
    int start = g->nodemap[u];
    int end = g->nodemap[u + 1];

    int i;
    for (i = start; i < end; i++) {
        if (g->edgelist[i] == v)
            return 1; /* Locals */
    }

    return 0;
}

float communityEv(community *c, graph *g) {
    if (c->ev == 0) { // if ev hasn't been calculated before
        matrix *sub = subgraph(g, c);
        c->ev = laplacianEv(sub);
        free(sub->rowmaj);
        free(sub);
    }
    return c->ev;
}

// Use LAPACK
float laplacianEv(matrix *adj) {
    matrix *laplacian = toLaplacian(adj);

    float ev = secondSmallestEv(laplacian);

    free(laplacian->rowmaj);
    free(laplacian);

    return ev;
}

// mergesort-like procedure spitting out adjacency list of induced subgraph
matrix *subgraph(graph *g, community *c) {
    matrix *adj = malloc(sizeof(matrix));
    adj->n = c->n;
    adj->rowmaj = calloc(adj->n * adj->n, sizeof(float));

    int i;
    for (i = 0; i < c->n; i++) { // for each node i in c
        int k = 0;
        int j = g->nodemap[c->nodes[i]];

        // while not running out of bounds in c's nodes or node i's edges:
        while (k < c->n && j < g->nodemap[c->nodes[i] + 1]) {
            int community_node = c->nodes[k];
            int graph_edge = g->edgelist[j];

            if (community_node < graph_edge)
                k++;
            else if (graph_edge < community_node)
                j++;
            else { // graph_edge == community_node
                adj->rowmaj[i * c->n + k] = 1;
                k++;
                j++;
            }
        }
    }

    return adj;
}

// create a new community c that is the union of a and b
// Kinda useless method that can be factored out
community *merge(community *a, community *b) {
    community *c = setUnion(a, b);
    c->ev = 0;
    c->id = a->id;
    return c;
}

// Counts edges going from a to b
int edgesBetweenSubsets(graph *g, community *a, community *b) {
    int count = 0; // number of edges
    int i = 0; // index in a
    int j = 0; // index in current node's neighbors
    int k = 0; // index in b

    for (i = 0; i < a->n; i++) {
        int start = g->nodemap[a->nodes[i]];
        int end = g->nodemap[a->nodes[i] + 1];
        j = start;
        k = 0;

        while (j < end || k < b->n) {
            int a_node = j == end ? INT_MAX : g->edgelist[j];
            int b_node = k == b->n ? INT_MAX : b->nodes[k];

            if (a_node < b_node) {
                j++;
            } else if (b_node < a_node) {
                k++;
            } else { // b_node == a_node
                count++;
                j++;
                k++;
            }
        }
    }

    return count;
}

void printCommunity(community *c) {
    printf("community %d: %d nodes", c->id, c->n);
    if (c->ev != 0)
        printf(", ev = %f", c->ev);
    printf(": ");

    int i;
    for (i = 0; i < c->n; i++)
        printf("%d ", c->nodes[i]);
    puts("");
}


void printGraph(graph *g) {
    printf("graph: %d nodes, %d edges\n", g->n, g->e);

    int i;
    printf("nodemap: ");
    for (i = 0; i < g->n && i < 1000; i++)
        printf("%d ", g->nodemap[i]);

    printf("\nedgelist: ");
    for (i = 0; i < g->e && i < 1000; i++)
        printf("%d ", g->edgelist[i]);

    puts("");
}

// maintains sorted order
// NOTE: optimizations.
// 1. sizeof(a+b) memory allocation could be improved
community *setUnion(community *a, community *b) {
    if (a == NULL)
        puts("COMMUNITY A IS NULL");
    if (b == NULL)
        puts("COMMUNITY B IS NULL");

    community *c = malloc(sizeof(community));
    c->nodes = calloc((a->n + b->n), sizeof(int));
    c->ev = 0;
    c->sibling = NULL;

    int i = 0; // index in c
    int j = 0; // index in a
    int k = 0; // index in b

    while (j < a->n || k < b->n) {
        int a_node = j == a->n ? INT_MAX : a->nodes[j];
        int b_node = k == b->n ? INT_MAX : b->nodes[k];

        if (a_node < b_node) {
            c->nodes[i] = a->nodes[j];
            j++;
        } else if (b_node < a_node) {
            c->nodes[i] = b->nodes[k];
            k++;
        } else { // b_node == a_node
            c->nodes[i] = a->nodes[j];
            j++;
            k++;
        }
        i++;
    }
    c->n = i;

    return c;
}

// copy+paste adaptation of setUnion
// computes a-b
community *setMinus(community *a, community *b) {
    community *c = malloc(sizeof(community));
    c->nodes = calloc(a->n, sizeof(int));
    c->ev = 0;
    c->sibling = NULL;

    int i = 0; // index in c
    int j = 0; // index in a
    int k = 0; // index in b

    while (j < a->n) {
        int a_node = j == a->n ? INT_MAX : a->nodes[j];
        int b_node = k == b->n ? INT_MAX : b->nodes[k];

        if (a_node < b_node) {
            c->nodes[i] = a->nodes[j];
            i++;
            j++;
        } else if (b_node < a_node) {
            k++;
        } else { // b_node == a_node
            j++;
            k++;
        }
    }
    c->n = i;

    return c;
}

// copy+paste adaptation of setUnion
// computes |a intersect b|
int commonElements(community *a, community *b) {
    int i = 0; // number of common elements
    int j = 0; // index in a
    int k = 0; // index in b

    while (j < a->n || k < b->n) {
        int a_node = j == a->n ? INT_MAX : a->nodes[j];
        int b_node = k == b->n ? INT_MAX : b->nodes[k];

        if (a_node < b_node) {
            j++;
        } else if (b_node < a_node) {
            k++;
        } else { // b_node == a_node
            i++;
            j++;
            k++;
        }
    }

    return i;
}

