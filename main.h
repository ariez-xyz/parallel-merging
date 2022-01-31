//
// Created by ariez on 29.05.20.
//

#ifndef MPICOMM_MAIN_H
#define MPICOMM_MAIN_H

typedef struct merge_result {
    int id1;
    int id2;
} merge_result;

c_index *prepare(char *graphFile, char *communitiesFile);

merge_result tryMergeRandomPair(c_index *ind);

community *checkPair(graph *g, community *c1, community *c2);

void setParams(double minNodeOverlapPerc, double minDisjointEdgesPerc, double minEvDelta);

#endif //MPICOMM_MAIN_H
