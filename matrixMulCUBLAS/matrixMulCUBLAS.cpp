////////////////////////////////////////////////////////////////////////////
//
// Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
//
// Please refer to the NVIDIA end user license agreement (EULA) associated
// with this source code for terms and conditions that govern your use of
// this software. Any use, reproduction, disclosure, or distribution of
// this software and related documentation outside the terms of the EULA
// is strictly prohibited.
//
////////////////////////////////////////////////////////////////////////////

//
// Matrix multiplication: C = A * B.
// Host code.
//
// This sample implements matrix multiplication as described in Chapter 3
// of the programming guide and uses the CUBLAS library to demonstrate
// the best performance.

// SOME PRECAUTIONS:
// IF WE WANT TO CALCULATE ROW-MAJOR MATRIX MULTIPLY C = A * B,
// WE JUST NEED CALL CUBLAS API IN A REVERSE ORDER: cublasSegemm(B, A)!
// The reason is explained as follows:

// CUBLAS library uses column-major storage, but C/C++ use row-major storage.
// When passing the matrix pointer to CUBLAS, the memory layout alters from
// row-major to column-major, which is equivalent to an implicit transpose.

// In the case of row-major C/C++ matrix A, B, and a simple matrix multiplication
// C = A * B, we can't use the input order like cublasSgemm(A, B)  because of
// implicit transpose. The actual result of cublasSegemm(A, B) is A(T) * B(T).
// If col(A(T)) != row(B(T)), equal to row(A) != col(B), A(T) and B(T) are not
// multipliable. Moreover, even if A(T) and B(T) are multipliable, the result C
// is a column-based cublas matrix, which means C(T) in C/C++, we need extra
// transpose code to convert it to a row-based C/C++ matrix.

// To solve the problem, let's consider our desired result C, a row-major matrix.
// In cublas format, it is C(T) actually (because of the implicit transpose).
// C = A * B, so C(T) = (A * B) (T) = B(T) * A(T). Cublas matrice B(T) and A(T)
// happen to be C/C++ matrice B and A (still because of the implicit transpose)!
// We don't need extra transpose code, we only need alter the input order!
//
// CUBLAS provides high-performance matrix multiplication.
// See also:
// V. Volkov and J. Demmel, "Benchmarking GPUs to tune dense linear algebra,"
// in Proc. 2008 ACM/IEEE Conf. on Supercomputing (SC '08),
// Piscataway, NJ: IEEE Press, 2008, pp. Art. 31:1-11.
//

// Utilities and system includes
#include <assert.h>
#include <helper_string.h>  // helper for shared functions common to CUDA Samples

// CUDA runtime
#include <cuda_runtime.h>
#include <cublas_v2.h>

// CUDA and CUBLAS functions
#include <helper_functions.h>
#include <helper_cuda.h>

#ifndef min
#define min(a,b) ((a < b) ? a : b)
#endif
#ifndef max
#define max(a,b) ((a > b) ? a : b)
#endif

typedef struct _matrixSize      // Optional Command-line multiplier for matrix sizes
{
    unsigned int uiWA, uiHA, uiWB, uiHB, uiWC, uiHC;
} sMatrixSize;


// Allocates a matrix with random float entries.
void randomInit(float *data, int size)
{
    for (int i = 0; i < size; ++i)
        data[i] = 1.0;
}



int N = 173056;
int M = 16;
int K = 27;
int lda = K;
int ldb = N;
int ldc = N;

void initializeCUDA(int argc, char **argv, int &devID, int &iSizeMultiple, sMatrixSize &matrix_size)
{
    // By default, we use device 0, otherwise we override the device ID based on what is provided at the command line
    cudaError_t error;
    devID = 0;

    devID = findCudaDevice(argc, (const char **)argv);

    cudaDeviceProp deviceProp;

    error = cudaGetDeviceProperties(&deviceProp, devID);

    if (error != cudaSuccess)
    {
        printf("cudaGetDeviceProperties returned error code %d, line(%d)\n", error, __LINE__);
        exit(EXIT_FAILURE);
    }

    printf("GPU Device %d: \"%s\" with compute capability %d.%d\n\n", devID, deviceProp.name, deviceProp.major, deviceProp.minor);

    int block_size = 32;
}



int matrixMultiply(int argc, char **argv, int devID, sMatrixSize &matrix_size)
{
    cudaDeviceProp deviceProp;

    checkCudaErrors(cudaGetDeviceProperties(&deviceProp, devID));

    int block_size = 32;

    // allocate host memory for matrices A and B
    unsigned int size_A = lda*M;
    unsigned int mem_size_A = sizeof(float) * size_A;
    float *h_A = (float *)malloc(mem_size_A);
    unsigned int size_B = ldb*K;
    unsigned int mem_size_B = sizeof(float) * size_B;
    float *h_B = (float *)malloc(mem_size_B);


    // initialize host memory
    randomInit(h_A, size_A);
    randomInit(h_B, size_B);

    // allocate device memory
    float *d_A, *d_B, *d_C;



    unsigned int size_C = ldc * M;
    unsigned int mem_size_C = sizeof(float) * size_C;

    // allocate host memory for the result
    float *h_C      = (float *) malloc(mem_size_C);
    float *h_CUBLAS = (float *) malloc(mem_size_C);
    


    checkCudaErrors(cudaMalloc((void **) &d_A, mem_size_A));
    checkCudaErrors(cudaMalloc((void **) &d_B, mem_size_B));
    checkCudaErrors(cudaMemcpy(d_A, h_A, mem_size_A, cudaMemcpyHostToDevice));
    checkCudaErrors(cudaMemcpy(d_B, h_B, mem_size_B, cudaMemcpyHostToDevice));
    checkCudaErrors(cudaMalloc((void **) &d_C, mem_size_C));
    randomInit(h_C, size_C);
    checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));

    // CUBLAS version 2.0
    {
        const float alpha = 1.0f;
        const float beta  = 1.0f;
        cublasHandle_t handle;

        checkCudaErrors(cublasCreate(&handle));

        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 173056,16,27, &alpha, d_B, 173056, d_A, 27, &beta, d_C, 173056));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 43264,32,144, &alpha, d_B, 43264, d_A, 144, &beta, d_C, 43264));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 10816,64,288, &alpha, d_B, 10816, d_A, 288, &beta, d_C, 10816));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 2704,128,576, &alpha, d_B, 2704, d_A, 576, &beta, d_C, 2704));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 676,256,1152, &alpha, d_B, 676, d_A, 1152, &beta, d_C, 676));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 169,512,2304, &alpha, d_B, 169, d_A, 2304, &beta, d_C, 169));
        // checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        // checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 169,1024,4608, &alpha, d_B, 169, d_A, 4608, &beta, d_C, 169));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 169,256,1024, &alpha, d_B, 169, d_A, 1024, &beta, d_C, 169));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 169,255,512, &alpha, d_B, 169, d_A, 512, &beta, d_C, 169));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 169,128,256, &alpha, d_B, 169, d_A, 256, &beta, d_C, 169));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 676,256,3456, &alpha, d_B, 676, d_A, 3456, &beta, d_C, 676));
        checkCudaErrors(cudaMemcpy(d_C, h_C, mem_size_C, cudaMemcpyHostToDevice));
        checkCudaErrors(cublasSgemm(handle, CUBLAS_OP_N, CUBLAS_OP_N, 676,255,256, &alpha, d_B, 676, d_A, 256, &beta, d_C, 676));


        checkCudaErrors(cudaMemcpy(h_CUBLAS, d_C, mem_size_C, cudaMemcpyDeviceToHost));
        // Destroy the handle
        checkCudaErrors(cublasDestroy(handle));
    }

    free(h_A);
    free(h_B);
    free(h_C);
    free(h_CUBLAS);
    checkCudaErrors(cudaFree(d_A));
    checkCudaErrors(cudaFree(d_B));
    checkCudaErrors(cudaFree(d_C));


    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Program main
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    printf("[Matrix Multiply CUBLAS] - Starting...\n");

    int devID = 0, sizeMult = 5;
    sMatrixSize matrix_size;

    initializeCUDA(argc, argv, devID, sizeMult, matrix_size);

    int matrix_result = matrixMultiply(argc, argv, devID, matrix_size);

    return matrix_result;
}