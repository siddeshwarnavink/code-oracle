#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct WordEmbedding {
    float *vector;
    size_t size;
} WordEmbedding;

int rnn(size_t vocab_size, size_t embedding_dim, size_t hidden_layersm, size_t epochs) {
    srand(time(NULL));

    /*
     * Initialize Embedding Layer
     */
    WordEmbedding *embedding_layer = malloc(vocab_size * sizeof(WordEmbedding));
    for(size_t i = 0; i < vocab_size; ++i) {
        embedding_layer[i].vector = malloc(embedding_dim * sizeof(float));
        for(size_t j = 0; j < embedding_dim; ++j)
            embedding_layer[i].vector[j] = ((float)rand() / 0.5f) * 0.01f;
    }

    printf("Model code!\n");

    /*
     * Free Embedding Layer
     */
    for(size_t i = 0; i < vocab_size; ++i) {
        free(embedding_layer[i].vector);
    }
    free(embedding_layer);

    return 0;
}
