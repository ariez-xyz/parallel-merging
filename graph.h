//
// Created by ariez on 4/4/20.
//

#ifndef MPICOMM_GRAPH_H
#define MPICOMM_GRAPH_H

#include "stdio.h"
#include "lib.h"

typedef struct {
    int n;
    int e;
    // dense list of all edges, each node's neighbor IDs stored contiguously. unsuitable for mutation
    int *edgelist;
    // nodemap[i] is the first index of node i's neighbor IDs in edgelist
    int *nodemap;
} graph;

typedef struct community {
    int id;
    float ev;
    int n;
    int *nodes; // list of nodes in this community
    struct community* sibling; // If this community is merged with another one, this points to the merge partner
    struct community* parent; // If this community's sibling is again merged, this points to the result of that.
    // So if the parent is not NULL, then this community is a "ghost entry" which only exists for the inverse index
} community;

void communityIsMessedUp(community *a);

int hasEdge(graph *g, int u, int v);

int edgesBetweenSubsets(graph *g, community *a, community *b);

graph *fromMetis(FILE *f);

matrix *subgraph(graph *g, community *c);

float communityEv(community *c, graph *g);

float laplacianEv(matrix *adj);

community *merge(community *a, community *b);

void printCommunity(community *c);

void printGraph(graph *g);

community *setUnion(community *a, community *b);

community *setMinus(community *a, community *b);

int commonElements(community *a, community *b);

#endif //MPICOMM_GRAPH_H
