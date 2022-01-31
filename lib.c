//
// Created by ariez on 5/1/20.
//

#include "lib.h"
#include <stdlib.h>
#include <stdio.h>
#include <lapacke.h>
#include <math.h>
#include <time.h>


void printMatrix(matrix *mat) {
    int i, j;
    printf("{\n");
    for (i = 0; i < mat->n; i++) {
        printf("\t");
        char *s = "";
        for (j = 0; j < mat->n; j++) {
            printf("%s%2.2f", s, mat->rowmaj[mat->n * i + j]);
            s = ", ";
        }
        printf("\n");
    }
    puts("}");
}

float secondSmallest(float array[], int n) {
    // find the second smallest real value
    float min = INFINITY;
    float min2 = INFINITY;
    int i;
    for (i = 0; i < n; i++) {
        if (array[i] < min) {
            min2 = min;
            min = array[i];
        } else if (array[i] < min2)
            min2 = array[i];
    }
    return min2;
}

// overrides matrix!
float secondSmallestEv(matrix *mat) {
    float *A = mat->rowmaj;

    lapack_int info, n, lda, m, ifail;
    n = mat->n;
    lda = mat->n; // leading dimension of A or sth

    // eigenvalues are saved here (real and imaginary values)
    float *wr = malloc(n * sizeof(float));
    //float *wi = malloc(n * sizeof(float));

    float range_from = 0;
    float range_to = 0;

    int index_from = 1;
    int index_to = 2;

    float abstol = 0; // error tolerance - set depending on minEvDelta

    // 'N' and 0 arguments are because we don't need eigenvectors
    //info = LAPACKE_sgeev(LAPACK_ROW_MAJOR, 'N', 'N', n, A, lda, wr, wi, 0, lda, 0, lda);
    info = LAPACKE_ssyevx(LAPACK_ROW_MAJOR, 'N', 'I', 'L', n, A, lda, range_from, range_to, index_from, index_to, abstol, &m, wr, 0, lda, &ifail);

    if (info != 0) {
        printf("ERROR FINDING EVS: info=%d\n", info);
    }

    // print all evs?
    //int i;
    //printf("eigenvalues: ");
    //for(i=0;i<mat->n;i++)
    //    printf("%f ", wr[i]);
    //printf("\nmin: %f\n", secondSmallest(wr, index_to - index_from + 1));

    float ret = wr[1];
    free(wr);
    return ret;
}

// caller frees!
matrix *toLaplacian(matrix *mat) {
    matrix *laplacian = malloc(sizeof(matrix));
    laplacian->n = mat->n;
    laplacian->rowmaj = calloc(mat->n * mat->n, sizeof(float)); // malloc would be fine too

    // Cache sums of rows
    float rowSums[mat->n];
    int i;
    for (i = 0; i < mat->n; i++) {
        rowSums[i] = sumRow(mat, i);
    }

    int j;
    for (i = 0; i < mat->n; i++)
        for (j = 0; j <= i; j++) {
            float laplacian_value = 0;
            if (i == j && rowSums[i] != 0) {
                laplacian_value = 1;
            } else if (mat->rowmaj[mat->n * i + j]) {
                laplacian_value = -1.0 / sqrt(rowSums[i] * rowSums[j]);
            }
            laplacian->rowmaj[mat->n * i + j] = laplacian_value;
        }

    return laplacian;
}

float sumRow(matrix *mat, int row) {
    if (row < 0 || row > mat->n) {
        printf("bad argument row (%d) passed to sumRow", row);
        return 0;
    }

    float c = 0;
    int i;
    for (i = 0; i < mat->n; i++)
        c += mat->rowmaj[row * mat->n + i];

    return c;
}

int randInt(int minInclusive, int maxExclusive) {
    int r = rand();
    int scale = maxExclusive - minInclusive;
    r %= scale;
    return minInclusive + r;
}

clock_t last;

void reset_clock() {
    last = clock();
}

void print_clock(char* s) {
    // TODO use higher resolution clock, e.g. https://www.cs.rutgers.edu/~pxk/416/notes/c-tutorials/gettime.html
    clock_t now = clock();
    printf("%s: %ld\n", s, (now - last));
    last = clock();
}
