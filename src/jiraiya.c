#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <float.h>

#include "trashman.h"

extern Item **global_items;

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

static Matrix *g_embedding_layer = NULL;
static DenseLayer *g_input_layer = NULL;
static DenseLayer *g_hidden_layer = NULL;
static Matrix *g_Wy = NULL;
static float *g_by = NULL;
static size_t g_vocab_size = 0, g_embedding_dim = 0, g_hidden_dim = 0;

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

void softmax(float *x, size_t len, float *out) {
    float max = x[0];
    for (size_t i = 1; i < len; ++i) if (x[i] > max) max = x[i];
    float sum = 0.0f;
    for (size_t i = 0; i < len; ++i) {
        out[i] = expf(x[i] - max);
        sum += out[i];
    }
    for (size_t i = 0; i < len; ++i) out[i] /= sum;
}

float cross_entropy_loss(float *pred, size_t target, size_t len) {
    float eps = 1e-8f;
    return -logf(pred[target] + eps);
}

void rnn_cell_forward(
    float *x_t, // [embedding_dim]
    float *h_prev, // [hidden_dim]
    DenseLayer *input_layer, // Wx
    DenseLayer *hidden_layer, // Wh
    float *bias, // [hidden_dim]
    float *h_t, // [hidden_dim] output
    size_t embedding_dim,
    size_t hidden_dim
) {
    for (size_t i = 0; i < hidden_dim; ++i) {
        float sum = bias[i];
        // Wx * x_t
        for (size_t j = 0; j < embedding_dim; ++j)
            sum += input_layer->weights->data[j][i] * x_t[j];
        // Wh * h_prev
        for (size_t j = 0; j < hidden_dim; ++j)
            sum += hidden_layer->weights->data[j][i] * h_prev[j];
        h_t[i] = tanhf(sum);
    }
}

void output_layer_forward(
    float *h_t, // [hidden_dim]
    Matrix *Wy, // [hidden_dim][vocab_size]
    float *by,  // [vocab_size]
    float *logits, // [vocab_size] output
    size_t hidden_dim,
    size_t vocab_size
) {
    for (size_t i = 0; i < vocab_size; ++i) {
        float sum = by[i];
        for (size_t j = 0; j < hidden_dim; ++j)
            sum += h_t[j] * Wy->data[j][i];
        logits[i] = sum;
    }
}

//
// Lex and BPE-encode a file
//
size_t *bpe_encode_file(const char *filepath, size_t *out_len) {
    FILE *file = fopen(filepath, "r");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *input_stream = malloc(file_size + 1);
    if (!input_stream) { fclose(file); return NULL; }
    fread(input_stream, 1, file_size, file);
    fclose(file);
    input_stream[file_size] = '\0';

    // Tokenize
    stb_lexer lexer;
    char string_store[1028];
    stb_c_lexer_init(&lexer, input_stream, input_stream + file_size, string_store, sizeof(string_store));
    int token = stb_c_lexer_get_token(&lexer);
    Item **items = NULL;
    while(token != 0) {
        Item *itm = malloc(sizeof(Item));
        itm->left = NULL;
        itm->right = NULL;
        itm->value.string = NULL;
        copy_into_item(itm, &lexer);
        arrput(items, itm);
        token = stb_c_lexer_get_token(&lexer);
    }
    free(input_stream);

    // BPE merge logic: repeatedly merge pairs using global_pairs until only BPE tokens remain
    // This mimics the logic in trashman.c
    int changed = 1;
    while (arrlenu(items) > 1 && changed) {
        changed = 0;
        for (size_t i = 0; i < arrlenu(global_pairs); ++i) {
            Pair *pair = global_pairs[i];
            for (size_t j = 0; j + 1 < arrlenu(items); ++j) {
                if (items_equal(items[j], pair->a) && items_equal(items[j+1], pair->b)) {
                    // Merge
                    Item *merged = malloc(sizeof(Item));
                    merged->id = pair->item_id;
                    merged->left = items[j];
                    merged->right = items[j+1];
                    merged->value.string = NULL;
                    // Remove j and j+1, insert merged at j
                    items[j] = merged;
                    arrdel(items, j+1);
                    changed = 1;
                    break;
                }
            }
            if (changed) break;
        }
    }

    // Now, each item in items is a BPE token (should correspond to global_items)
    size_t *ids = NULL;
    for (size_t i = 0; i < arrlenu(items); ++i) {
        // Find the id in global_items
        size_t found = 0;
        for (size_t j = 0; j < arrlenu(global_items); ++j) {
            if (items_equal(items[i], global_items[j])) {
                arrput(ids, global_items[j]->id);
                found = 1;
                break;
            }
        }
        if (!found) arrput(ids, 0); // fallback to 0 if not found
    }

    /*
    printf("[BPE ENCODE] Encoded IDs: ");
    for (size_t i = 0; i < arrlenu(ids); ++i) {
        printf("%zu ('%s') ", ids[i], bpe_token_string(ids[i]));
    }
    printf("\n");
    */

    for (size_t i = 0; i < arrlenu(items); ++i) free(items[i]);
    arrfree(items);
    *out_len = arrlenu(ids);
    return ids;
}

//
// Load all .js files in dataset dir and concatenate BPE ids
//
size_t *load_bpe_dataset(const char *dataset_dir, size_t *total_len) {
    DIR *dir = opendir(dataset_dir);
    if (!dir) return NULL;
    struct dirent *entry;
    size_t *all_ids = NULL;
    char path[1024];
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".js")) {
            snprintf(path, sizeof(path), "%s/%s", dataset_dir, entry->d_name);
            size_t len = 0;
            size_t *ids = bpe_encode_file(path, &len);
            for (size_t i = 0; i < len; ++i) arrput(all_ids, ids[i]);
            arrfree(ids);
        }
    }
    closedir(dir);
    *total_len = arrlenu(all_ids);
    return all_ids;
}

void save_model(const char *path, Matrix *embedding_layer, DenseLayer *input_layer, DenseLayer *hidden_layer, Matrix *Wy, float *by, size_t vocab_size, size_t embedding_dim, size_t hidden_dim) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[ERROR] Could not open model file for writing: %s\n", path);
        return;
    }
    //
    // Save dimensions
    //
    fwrite(&vocab_size, sizeof(size_t), 1, f);
    fwrite(&embedding_dim, sizeof(size_t), 1, f);
    fwrite(&hidden_dim, sizeof(size_t), 1, f);

    //
    // Embedding layer
    //
    for (size_t i = 0; i < vocab_size; ++i)
        fwrite(embedding_layer->data[i], sizeof(float), embedding_dim, f);
    //
    // Input layer weights and bias
    //
    for (size_t i = 0; i < embedding_dim; ++i)
        fwrite(input_layer->weights->data[i], sizeof(float), hidden_dim, f);
    fwrite(input_layer->bias, sizeof(float), hidden_dim, f);

    //
    // Hidden layer weights and bias
    //
    for (size_t i = 0; i < hidden_dim; ++i)
        fwrite(hidden_layer->weights->data[i], sizeof(float), hidden_dim, f);
    fwrite(hidden_layer->bias, sizeof(float), hidden_dim, f);

    //
    // Output layer weights and bias
    //
    for (size_t i = 0; i < hidden_dim; ++i)
        fwrite(Wy->data[i], sizeof(float), vocab_size, f);
    fwrite(by, sizeof(float), vocab_size, f);
    fclose(f);
    printf("[INFO] Model saved to %s\n", path);
}

int rnn(size_t vocab_size, size_t embedding_dim, size_t hidden_dim, size_t epochs, const char *model_path) {
    srand(time(NULL));

    Matrix *embedding_layer = malloc(sizeof(Matrix));
    if (!embedding_layer || mat_rand_create(embedding_layer, vocab_size, embedding_dim) > 0) {
        fprintf(stderr, "[ERROR] Failed to initialize embedding layer\n");
        goto cleanup;
    }
    DenseLayer *input_layer = malloc(sizeof(DenseLayer));
    if (!input_layer || dense_create(input_layer, embedding_dim, hidden_dim) > 0) {
        fprintf(stderr, "[ERROR] Failed to initialize input layer\n");
        goto cleanup;
    }
    DenseLayer *hidden_layer = malloc(sizeof(DenseLayer));
    if (!hidden_layer || dense_create(hidden_layer, hidden_dim, hidden_dim) > 0) {
        fprintf(stderr, "[ERROR] Failed to initialize hidden layer\n");
        goto cleanup;
    }
    float *bias = calloc(hidden_dim, sizeof(float));
    if (!bias) {
        fprintf(stderr, "[ERROR] Failed to initialize bias\n");
        goto cleanup;
    }

    Matrix *Wy = malloc(sizeof(Matrix));
    if (!Wy || mat_rand_create(Wy, hidden_dim, vocab_size) > 0) {
        fprintf(stderr, "[ERROR] Failed to initialize output layer\n");
        goto cleanup;
    }
    float *by = calloc(vocab_size, sizeof(float));
    if (!by) {
        fprintf(stderr, "[ERROR] Failed to initialize output bias\n");
        goto cleanup;
    }

    printf("[INFO] Running RNN training...\n");
    size_t sequence_length = 32;
    float learning_rate = 0.01f;

    //
    // Load dataset
    //
    size_t dataset_len = 0;
    size_t *dataset = load_bpe_dataset(".dataset", &dataset_len);
    if (!dataset || dataset_len < sequence_length + 1) {
        fprintf(stderr, "[ERROR] Not enough BPE data for training\n");
        goto cleanup;
    }
    float *h_prev = calloc(hidden_dim, sizeof(float));
    float *h_t = calloc(hidden_dim, sizeof(float));
    float *logits = calloc(vocab_size, sizeof(float));
    float *probs = calloc(vocab_size, sizeof(float));
    float *dlogits = calloc(vocab_size, sizeof(float));
    float *dh = calloc(hidden_dim, sizeof(float));
    float *dx = calloc(embedding_dim, sizeof(float));

    //
    // BPTT gradients
    //
    float **dh_t = malloc(sequence_length * sizeof(float*));
    float **dh_next = malloc(sequence_length * sizeof(float*));
    for (size_t t = 0; t < sequence_length; ++t) {
        dh_t[t] = calloc(hidden_dim, sizeof(float));
        dh_next[t] = calloc(hidden_dim, sizeof(float));
    }

    //
    // Gradients for hidden weights and bias
    //
    Matrix *dWh = malloc(sizeof(Matrix));
    mat_create_zero(dWh, hidden_dim, hidden_dim);
    float *dbh = calloc(hidden_dim, sizeof(float));
    size_t num_batches = (dataset_len - sequence_length) / sequence_length;
    size_t *batch_indices = malloc(num_batches * sizeof(size_t));
    for (size_t i = 0; i < num_batches; ++i) batch_indices[i] = i;
    size_t correct = 0, total = 0;
    for (size_t epoch = 0; epoch < epochs; ++epoch) {
        //
        // Shuffle batches
        //
        for (size_t i = num_batches - 1; i > 0; --i) {
            size_t j = rand() % (i + 1);
            size_t tmp = batch_indices[i];
            batch_indices[i] = batch_indices[j];
            batch_indices[j] = tmp;
        }
        float epoch_loss = 0.0f;
        for (size_t b = 0; b < num_batches; ++b) {
            size_t batch = batch_indices[b];
            size_t start = batch * sequence_length;
            size_t *input_seq = &dataset[start];
            size_t *target_seq = &dataset[start + 1];
            for (size_t i = 0; i < hidden_dim; ++i) h_prev[i] = 0.0f;

            //
            // Store activations for BPTT
            //
            float **h_states = malloc((sequence_length + 1) * sizeof(float*));
            for (size_t t = 0; t <= sequence_length; ++t) h_states[t] = calloc(hidden_dim, sizeof(float));
            memcpy(h_states[0], h_prev, hidden_dim * sizeof(float));

            //
            // Forward pass
            //
            for (size_t t = 0; t < sequence_length; ++t) {
                float *x_t = embedding_layer->data[input_seq[t]];
                rnn_cell_forward(x_t, h_states[t], input_layer, hidden_layer, bias, h_states[t+1], embedding_dim, hidden_dim);
                output_layer_forward(h_states[t+1], Wy, by, logits, hidden_dim, vocab_size);
                softmax(logits, vocab_size, probs);
                float loss = cross_entropy_loss(probs, target_seq[t], vocab_size);
                epoch_loss += loss;

                //
                // Accuracy
                //
                size_t pred = 0;
                float max_prob = probs[0];
                for (size_t i = 1; i < vocab_size; ++i) {
                    if (probs[i] > max_prob) { max_prob = probs[i]; pred = i; }
                }
                if (pred == target_seq[t]) correct++;
                total++;
            }

            //
            // Zero gradients
            //
            mat_create_zero(dWh, hidden_dim, hidden_dim);
            memset(dbh, 0, hidden_dim * sizeof(float));

            //
            // Backward pass (BPTT)
            //
            memset(dh, 0, hidden_dim * sizeof(float));
            for (int t = sequence_length - 1; t >= 0; --t) {
                float *x_t = embedding_layer->data[input_seq[t]];
                float *h_t_ = h_states[t+1];
                float *h_prev_ = h_states[t];
                output_layer_forward(h_t_, Wy, by, logits, hidden_dim, vocab_size);
                softmax(logits, vocab_size, probs);
                for (size_t i = 0; i < vocab_size; ++i)
                    dlogits[i] = probs[i];
                dlogits[target_seq[t]] -= 1.0f;
                // Output layer gradients (as before)
                for (size_t i = 0; i < vocab_size; ++i) {
                    for (size_t j = 0; j < hidden_dim; ++j)
                        Wy->data[j][i] -= learning_rate * dlogits[i] * h_t_[j];
                    by[i] -= learning_rate * dlogits[i];
                }
                // dh = Wy * dlogits + dh_next
                for (size_t j = 0; j < hidden_dim; ++j) {
                    dh[j] = 0.0f;
                    for (size_t i = 0; i < vocab_size; ++i)
                        dh[j] += Wy->data[j][i] * dlogits[i];
                    dh[j] += dh_next[t][j];
                }
                // Backprop through tanh
                for (size_t j = 0; j < hidden_dim; ++j)
                    dh[j] *= (1.0f - h_t_[j] * h_t_[j]);
                // Accumulate gradients for hidden weights and bias
                for (size_t i = 0; i < hidden_dim; ++i) {
                    for (size_t j = 0; j < hidden_dim; ++j)
                        dWh->data[j][i] += h_prev_[j] * dh[i];
                    dbh[i] += dh[i];
                }
                // Accumulate gradients for input layer (as before)
                for (size_t i = 0; i < embedding_dim; ++i)
                    for (size_t j = 0; j < hidden_dim; ++j)
                        input_layer->weights->data[i][j] -= learning_rate * dh[j] * x_t[i];
                for (size_t i = 0; i < hidden_dim; ++i)
                    bias[i] -= learning_rate * dh[i];
                // Save dh for next step
                if (t > 0) memcpy(dh_next[t-1], dh, hidden_dim * sizeof(float));
            }
            // Gradient clipping for hidden weights and bias
            float clip = 5.0f;
            for (size_t i = 0; i < hidden_dim; ++i) {
                if (dbh[i] > clip) dbh[i] = clip;
                if (dbh[i] < -clip) dbh[i] = -clip;
                for (size_t j = 0; j < hidden_dim; ++j) {
                    if (dWh->data[j][i] > clip) dWh->data[j][i] = clip;
                    if (dWh->data[j][i] < -clip) dWh->data[j][i] = -clip;
                }
            }
            // Update hidden layer weights and bias
            for (size_t i = 0; i < hidden_dim; ++i) {
                for (size_t j = 0; j < hidden_dim; ++j)
                    hidden_layer->weights->data[j][i] -= learning_rate * dWh->data[j][i];
                hidden_layer->bias[i] -= learning_rate * dbh[i];
            }
            // Free BPTT memory for this batch
            for (size_t t = 0; t <= sequence_length; ++t) free(h_states[t]);
            free(h_states);
        }
        printf("[INFO] Epoch %zu, avg loss: %.4f\n", epoch + 1, epoch_loss / (num_batches * sequence_length));
    }
    float accuracy = (total > 0) ? (100.0f * correct / total) : 0.0f;
    printf("[INFO] Training complete. Accuracy: %.2f%% (%zu/%zu)\n", accuracy, correct, total);
    if (model_path) {
        save_model(model_path, embedding_layer, input_layer, hidden_layer, Wy, by, vocab_size, embedding_dim, hidden_dim);
    }

    arrfree(dataset); dataset = NULL;
    free(h_prev); h_prev = NULL;
    free(h_t); h_t = NULL;
    free(logits); logits = NULL;
    free(probs); probs = NULL;
    free(dlogits); dlogits = NULL;
    free(dh); dh = NULL;
    free(dx); dx = NULL;
    for (size_t t = 0; t < sequence_length; ++t) { free(dh_t[t]); free(dh_next[t]); }
    free(dh_t); free(dh_next);
    mat_free(dWh); free(dWh); free(dbh); free(batch_indices);

cleanup:
    if (embedding_layer) {
        mat_free(embedding_layer);
        free(embedding_layer);
        embedding_layer = NULL;
    }
    if (input_layer) {
        dense_free(input_layer);
        free(input_layer);
        input_layer = NULL;
    }
    if (hidden_layer) {
        dense_free(hidden_layer);
        free(hidden_layer);
        hidden_layer = NULL;
    }
    if (Wy) {
        mat_free(Wy);
        free(Wy);
        Wy = NULL;
    }
    if (by) { free(by); by = NULL; }
    if (bias) { free(bias); bias = NULL; }
    return 0;
}

int load_model(const char *path) {
    printf("[INFO] Loading model from %s\n", path);

    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    fread(&g_vocab_size, sizeof(size_t), 1, f);
    fread(&g_embedding_dim, sizeof(size_t), 1, f);
    fread(&g_hidden_dim, sizeof(size_t), 1, f);

    g_embedding_layer = malloc(sizeof(Matrix));
    mat_create_zero(g_embedding_layer, g_vocab_size, g_embedding_dim);
    for (size_t i = 0; i < g_vocab_size; ++i)
        fread(g_embedding_layer->data[i], sizeof(float), g_embedding_dim, f);
    g_input_layer = malloc(sizeof(DenseLayer));
    g_input_layer->weights = malloc(sizeof(Matrix));
    mat_create_zero(g_input_layer->weights, g_embedding_dim, g_hidden_dim);
    for (size_t i = 0; i < g_embedding_dim; ++i)
        fread(g_input_layer->weights->data[i], sizeof(float), g_hidden_dim, f);
    g_input_layer->bias = malloc(g_hidden_dim * sizeof(float));
    fread(g_input_layer->bias, sizeof(float), g_hidden_dim, f);
    g_hidden_layer = malloc(sizeof(DenseLayer));
    g_hidden_layer->weights = malloc(sizeof(Matrix));
    mat_create_zero(g_hidden_layer->weights, g_hidden_dim, g_hidden_dim);
    for (size_t i = 0; i < g_hidden_dim; ++i)
        fread(g_hidden_layer->weights->data[i], sizeof(float), g_hidden_dim, f);
    g_hidden_layer->bias = malloc(g_hidden_dim * sizeof(float));
    fread(g_hidden_layer->bias, sizeof(float), g_hidden_dim, f);
    g_Wy = malloc(sizeof(Matrix));
    mat_create_zero(g_Wy, g_hidden_dim, g_vocab_size);
    for (size_t i = 0; i < g_hidden_dim; ++i)
        fread(g_Wy->data[i], sizeof(float), g_vocab_size, f);
    g_by = malloc(g_vocab_size * sizeof(float));
    fread(g_by, sizeof(float), g_vocab_size, f);
    fclose(f);
    return 0;
}

//
// Predict next token given input string (BPE-encoded)
//
int rnn_predict(const char *input, char *output, size_t output_len) {
    size_t input_len = strlen(input);
    stb_lexer lexer;
    char string_store[1028];
    stb_c_lexer_init(&lexer, input, input + input_len, string_store, sizeof(string_store));
    int token = stb_c_lexer_get_token(&lexer);
    Item **items = NULL;
    while(token != 0) {
        Item *itm = malloc(sizeof(Item));
        copy_into_item(itm, &lexer);
        arrput(items, itm);
        token = stb_c_lexer_get_token(&lexer);
    }
    size_t *ids = NULL;
    for (size_t i = 0; i < arrlenu(items); ++i) {
        int found = 0;
        for (size_t j = 0; j < arrlenu(global_pairs); ++j) {
            if (items_equal(items[i], global_pairs[j]->a)) {
                arrput(ids, global_pairs[j]->item_id);
                found = 1;
                break;
            }
        }
        if (!found) arrput(ids, 0);
    }
    for (size_t i = 0; i < arrlenu(items); ++i) free(items[i]);
    arrfree(items);

    //
    // Predict next token(s)
    //
    float *h_prev = calloc(g_hidden_dim, sizeof(float));
    float *h_t = calloc(g_hidden_dim, sizeof(float));
    float *logits = calloc(g_vocab_size, sizeof(float));
    float *probs = calloc(g_vocab_size, sizeof(float));
    for (size_t t = 0; t < arrlenu(ids); ++t) {
        float *x_t = g_embedding_layer->data[ids[t]];
        rnn_cell_forward(x_t, h_prev, g_input_layer, g_hidden_layer, g_input_layer->bias, h_t, g_embedding_dim, g_hidden_dim);
        for (size_t i = 0; i < g_hidden_dim; ++i) h_prev[i] = h_t[i];
    }

    //
    // Predict next token
    //
    output[0] = '\0';
    output_len = output_len > 0 ? output_len : 1;
    output[output_len-1] = '\0';
    output_layer_forward(h_t, g_Wy, g_by, logits, g_hidden_dim, g_vocab_size);
    softmax(logits, g_vocab_size, probs);

    //
    // Find most probable token id
    //
    size_t pred = 0;
    float max_prob = probs[0];
    for (size_t i = 1; i < g_vocab_size; ++i) {
        if (probs[i] > max_prob) { max_prob = probs[i]; pred = i; }
    }

    // Clamp pred to valid range
    if (pred >= g_vocab_size) pred = 0;

    /*
       printf("[RNN PREDICT] Predicted token id: %zu, string: '%s'\n", pred, bpe_token_string(pred));
       snprintf(output, output_len, "%zu", pred);
       */

    free(h_prev); free(h_t); free(logits); free(probs); arrfree(ids);
    return 0;
}
