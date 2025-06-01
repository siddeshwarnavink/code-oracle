#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct Matrix {
    float **data;
    size_t row;
    size_t col;
} Matrix;

typedef struct DenseLayer {
    Matrix *weights;
    float *bias;
    size_t input_size;
    size_t output_size;
} DenseLayer;

int mat_rand_create(Matrix *mat, size_t row, size_t col) {
    mat->row = row;
    mat->col = col;
    mat->data = malloc(row * sizeof(float *));
    for(size_t i = 0; i < row; ++i) {
        mat->data[i] = malloc(col * sizeof(float));
        if(mat->data[i] == NULL) return 1;

        for (size_t j = 0; j < col; ++j) {
            mat->data[i][j] = ((float)rand() / RAND_MAX - 0.5f) * 0.01f;
        }
    }
    return 0;
}

int mat_create_zero(Matrix *mat, size_t row, size_t col) {
    mat->row = row;
    mat->col = col;
    mat->data = malloc(row * sizeof(float *));
    for(size_t i = 0; i < row; ++i) {
        mat->data[i] = malloc(col * sizeof(float));
        if(mat->data[i] == NULL) return 1;

        for (size_t j = 0; j < col; ++j) {
            mat->data[i][j] = 0.0f;
        }
    }
    return 0;
}

int mat_mul(Matrix *res, Matrix *a, Matrix *b) {
    if(a->col != b->row) return 1;

    if(mat_create_zero(res, a->row, b->col) > 0) return 1;

    for(size_t i = 0; i < res->row; ++i) {
        for (size_t j = 0; j < res->col; ++j) {
            res->data[i][j] = 0.0f;
            for (size_t k = 0; k < a->col; ++k)
                res->data[i][j] += a->data[i][k] * b->data[k][j];
        }
    }

    return 0;
}

void mat_free(Matrix *mat) {
    for (size_t i = 0; i < mat->row; ++i)
        free(mat->data[i]);
    free(mat->data);
}

int dense_create(DenseLayer *dl, size_t input_size, size_t output_size) {
    dl->input_size = input_size;
    dl->output_size = output_size;

    if(mat_rand_create(dl->weights, input_size, output_size) > 0) return 1;

    dl->bias = malloc(output_size * sizeof(float));
    if(dl->bias == NULL) return 1;

    for (size_t i = 0; i < output_size; ++i)
        dl->bias[i] = 0.0f;

    return 0;
}

void dense_free(DenseLayer *dl) {
    mat_free(dl->weights);
    free(dl->bias);
}

int rnn(size_t vocab_size, size_t embedding_dim, size_t hidden_dim, size_t epochs) {
    srand(time(NULL));

    Matrix *embedding_layer = NULL;
    DenseLayer *input_layer = NULL;
    DenseLayer *hidden_layer = NULL;
    float *bias = NULL;

    embedding_layer = malloc(sizeof(Matrix));
    if (!embedding_layer || mat_rand_create(embedding_layer, vocab_size, embedding_dim) > 0) {
        fprintf(stderr, "[ERROR] Failed to initialize embedding layer\n");
        goto cleanup;
    }

    input_layer = malloc(sizeof(DenseLayer));
    if (!input_layer || dense_create(input_layer, embedding_dim, hidden_dim) > 0) {
        fprintf(stderr, "[ERROR] Failed to initialize input layer\n");
        goto cleanup;
    }

    hidden_layer = malloc(sizeof(DenseLayer));
    if (!hidden_layer || dense_create(hidden_layer, hidden_dim, hidden_dim) > 0) {
        fprintf(stderr, "[ERROR] Failed to initialize hidden layer\n");
        goto cleanup;
    }

    bias = calloc(hidden_dim, sizeof(float));
    if (!bias) {
        fprintf(stderr, "[ERROR] Failed to initialize bias\n");
        goto cleanup;
    }

    printf("[INFO] Running RNN...\n");
    // TODO: Training logic

cleanup:
    if (embedding_layer) {
        mat_free(embedding_layer);
        free(embedding_layer);
    }

    if (input_layer) {
        dense_free(input_layer);
        free(input_layer);
    }

    if (hidden_layer) {
        dense_free(hidden_layer);
        free(hidden_layer);
    }

    if(bias) { 
        free(bias);
    }

    return (bias && hidden_layer && input_layer && embedding_layer) ? 0 : 1;
}
