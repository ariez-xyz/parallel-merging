//
// Created by ariez on 5/1/20.
//

#ifndef MPICOMM_LIB_H
#define MPICOMM_LIB_H


typedef struct {
    int n; // a *square* matrix
    float *rowmaj;
} matrix;

// find second smallest value in a double array
float secondSmallest(float array[], int n);

float secondSmallestEv(matrix *mat);

matrix *toLaplacian(matrix *mat);

float sumRow(matrix *mat, int row);

void printMatrix(matrix *mat);

int randInt(int minInclusive, int maxExclusive);

void reset_clock();

void print_clock(char* s);

#endif //MPICOMM_LIB_H
