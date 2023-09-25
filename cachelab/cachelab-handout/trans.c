/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, x;
    int tmp[8];
    if (M == 32)
    {
        for (i = 0; i < N; i += 8)
        {
            for (j = 0; j < M; j += 8)
            {
                for (x = i; x < i + 8; x++)
                {
                    tmp[0] = A[x][j];
                    tmp[1] = A[x][j + 1];
                    tmp[2] = A[x][j + 2];
                    tmp[3] = A[x][j + 3];
                    tmp[4] = A[x][j + 4];
                    tmp[5] = A[x][j + 5];
                    tmp[6] = A[x][j + 6];
                    tmp[7] = A[x][j + 7];
                    B[j][x] = tmp[0];
                    B[j + 1][x] = tmp[1];
                    B[j + 2][x] = tmp[2];
                    B[j + 3][x] = tmp[3];
                    B[j + 4][x] = tmp[4];
                    B[j + 5][x] = tmp[5];
                    B[j + 6][x] = tmp[6];
                    B[j + 7][x] = tmp[7];
                }
            }
        }
    }
    else if (M == 64)
    {
        for (i = 0; i < N; i += 8)
        {
            for (j = 0; j < M; j += 8)
            {
                for (k = 0; k < 8 / 2; k++)
                {
                    // A top left
                    tmp[0] = A[k + i][j];
                    tmp[1] = A[k + i][j + 1];
                    tmp[2] = A[k + i][j + 2];
                    tmp[3] = A[k + i][j + 3];

                    // copy
                    // A top right
                    tmp[4] = A[k + i][j + 4];
                    tmp[5] = A[k + i][j + 5];
                    tmp[6] = A[k + i][j + 6];
                    tmp[7] = A[k + i][j + 7];

                    // B top left
                    B[j][k + i] = tmp[0];
                    B[j + 1][k + i] = tmp[1];
                    B[j + 2][k + i] = tmp[2];
                    B[j + 3][k + i] = tmp[3];

                    // copy
                    // B top right
                    B[j + 0][k + 4 + i] = tmp[4];
                    B[j + 1][k + 4 + i] = tmp[5];
                    B[j + 2][k + 4 + i] = tmp[6];
                    B[j + 3][k + 4 + i] = tmp[7];
                }
                for (k = 0; k < 8 / 2; k++)
                {
                    // step 1 2
                    tmp[0] = A[i + 4][j + k], tmp[4] = A[i + 4][j + k + 4];
                    tmp[1] = A[i + 5][j + k], tmp[5] = A[i + 5][j + k + 4];
                    tmp[2] = A[i + 6][j + k], tmp[6] = A[i + 6][j + k + 4];
                    tmp[3] = A[i + 7][j + k], tmp[7] = A[i + 7][j + k + 4];
                    // step 3
                    x = B[j + k][i + 4], B[j + k][i + 4] = tmp[0], tmp[0] = x;
                    x = B[j + k][i + 5], B[j + k][i + 5] = tmp[1], tmp[1] = x;
                    x = B[j + k][i + 6], B[j + k][i + 6] = tmp[2], tmp[2] = x;
                    x = B[j + k][i + 7], B[j + k][i + 7] = tmp[3], tmp[3] = x;
                    // step 4
                    B[j + k + 4][i + 0] = tmp[0], B[j + k + 4][i + 4 + 0] = tmp[4];
                    B[j + k + 4][i + 1] = tmp[1], B[j + k + 4][i + 4 + 1] = tmp[5];
                    B[j + k + 4][i + 2] = tmp[2], B[j + k + 4][i + 4 + 2] = tmp[6];
                    B[j + k + 4][i + 3] = tmp[3], B[j + k + 4][i + 4 + 3] = tmp[7];
                }
            }
        }
    }
    else if (M == 61)
    {
        for (i = 0; i < N; i += 8)
        {
            for (j = 0; j < M; j += 23)
            {
                if (i + 8 <= N && j + 23 <= M)
                {
                    for (k = j; k < j + 23; k++)
                    {
                        tmp[0] = A[i][k];
                        tmp[1] = A[i + 1][k];
                        tmp[2] = A[i + 2][k];
                        tmp[3] = A[i + 3][k];
                        tmp[4] = A[i + 4][k];
                        tmp[5] = A[i + 5][k];
                        tmp[6] = A[i + 6][k];
                        tmp[7] = A[i + 7][k];
                        B[k][i + 0] = tmp[0];
                        B[k][i + 1] = tmp[1];
                        B[k][i + 2] = tmp[2];
                        B[k][i + 3] = tmp[3];
                        B[k][i + 4] = tmp[4];
                        B[k][i + 5] = tmp[5];
                        B[k][i + 6] = tmp[6];
                        B[k][i + 7] = tmp[7];
                    }
                }
                else
                {
                    for (k = i; k < ((i + 8 < N) ? (i + 8) : N); k++)
                    {
                        for (tmp[0] = j; tmp[0] < ((j + 23 < M) ? (j + 23) : M); tmp[0]++)
                        {
                            B[tmp[0]][k] = A[k][tmp[0]];
                        }
                    }
                }
            }
        }
    }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; j++)
        {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);
}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; ++j)
        {
            if (A[i][j] != B[j][i])
            {
                return 0;
            }
        }
    }
    return 1;
}
