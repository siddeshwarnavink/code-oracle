#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#define STB_C_LEXER_IMPLEMENTATION
#define STB_DS_IMPLEMENTATION
#include "stb_c_lexer.h"
#include "stb_ds.h"
#include "tree_sitter/api.h"
#include "tree-sitter-javascript.h"

typedef struct Item {
	size_t id; // Only for nested items.
	struct Item *left;
	struct Item *right;
	stb_lexer value;
} Item;

typedef struct Pair {
	Item *a;
	Item *b;
	size_t item_id;
} Pair;

typedef struct StringChanges {
	size_t start;
	size_t end;
	char text[32];
} StringChanges;

typedef struct Variable {
    char *original_name;
    char *new_name;
    struct Variable *next;
} Variable;

typedef struct Scope {
    Variable *variables;
    struct Scope *parent;
} Scope;


size_t var_count = 0, item_counter = 0;
Pair **global_pairs = NULL;
Item **global_items = NULL;

size_t tokens_equal(stb_lexer a, stb_lexer b) {
	if(a.token == b.token) {
		switch(a.token) {
		case CLEX_id:
		case CLEX_dqstring:
		case CLEX_sqstring:
			return strcmp(a.string, b.string) == 0;
		case CLEX_intlit:
			return a.int_number == b.int_number;
		case CLEX_floatlit:
			return a.real_number == b.real_number;
		default:
			return 1;
		}
	}
	return 0;
}

size_t items_equal(Item *a, Item *b) {
	if (a == NULL || b == NULL) {
		assert(1);
	}

	if(a->left && b->left && a->right && b->right)
		return a->id == b->id;

	if(!a->left && !b->left && !a->right && !b->right)
		return tokens_equal(a->value, b->value);

	return 0;
}

void copy_into_item(Item *a, stb_lexer *b) {
	a->value.token = b->token;
	switch(b->token) {
	case CLEX_id:
	case CLEX_dqstring:
	case CLEX_sqstring:
		a->value.string =  malloc(strlen(b->string) + 1);
		strcpy(a->value.string, b->string);
		break;
	case CLEX_intlit:
		a->value.int_number = b->int_number;
		break;
	case CLEX_floatlit:
		a->value.real_number = b->real_number;
		break;
	}
}

//
// NOTE: Left and right should be freed manually.
//
void free_item(Item *itm) {
	assert(itm != NULL);
	if(itm->value.string != NULL)
		free(itm->value.string);
	free(itm);
}

void print_item(Item *itm) {
	if(itm->left != NULL && itm->right != NULL) {
		printf("(");
		print_item(itm->left);
		printf(" ");
		print_item(itm->right);
		printf(")");
	} else {
		switch(itm->value.token) {
		case CLEX_id:
		case CLEX_dqstring:
		case CLEX_sqstring:
			printf("%s", itm->value.string);
			break;
		case CLEX_intlit:
			printf("%ld", itm->value.int_number);
			break;
		case CLEX_floatlit:
			printf("%lf", itm->value.real_number);
			break;
		default:
			if(itm->value.token < 256)
				printf("%c", (char)itm->value.token);
			else
				printf("%ld", itm->value.token);
			break;
		}
	}
}

int pair_already_merged(Pair *pairs, Item *a, Item *b) {
	if(pairs == NULL || a == NULL || b == NULL)
		assert(1);

	if(arrlenu(pairs) == 0)
		assert(1);

	for(size_t i = 0; i < arrlenu(pairs); ++i) {
		if((items_equal(pairs[i].a, a) && items_equal(pairs[i].b, b))
		   || (items_equal(pairs[i].a, b) && items_equal(pairs[i].b, a)))
			return (int)i;
	}
	return -1;
}

Scope* create_scope(Scope *parent) {
    Scope *scope = (Scope*)malloc(sizeof(Scope));
    scope->variables = NULL;
    scope->parent = parent;
    return scope;
}

void add_variable(Scope *scope, const char *original_name, const char *new_name) {
    Variable *var = (Variable*)malloc(sizeof(Variable));
    var->original_name = strdup(original_name);
    var->new_name = strdup(new_name);
    var->next = scope->variables;
    scope->variables = var;
}

const char* find_variable(Scope *scope, const char *name) {
    while (scope != NULL) {
        Variable *current = scope->variables;
        while (current != NULL) {
            if (strcmp(current->original_name, name) == 0) {
                return current->new_name;
            }
            current = current->next;
        }
        scope = scope->parent;
    }
    return NULL;
}

int is_scope_boundary(const char *type) {
    return strcmp(type, "statement_block") == 0 ||
		strcmp(type, "function_declaration") == 0 ||
		strcmp(type, "method_definition") == 0 ||
		strcmp(type, "class_body") == 0 ||
		strcmp(type, "for_statement") == 0 ||
		strcmp(type, "while_statement") == 0 ||
		strcmp(type, "if_statement") == 0;
}

void rename_variables(TSNode node, const char *source_code, StringChanges ***changes, Scope *current_scope) {
    const char *node_type = ts_node_type(node);

    Scope *new_scope = NULL;
    if (is_scope_boundary(node_type)) {
        new_scope = create_scope(current_scope);
        current_scope = new_scope;
    }

    if (strcmp(node_type, "lexical_declaration") == 0 ||
        strcmp(node_type, "variable_declaration") == 0) {

        TSNode declarator = ts_node_named_child(node, 0);
        if (strcmp(ts_node_type(declarator), "variable_declarator") == 0) {
            TSNode identifier = ts_node_named_child(declarator, 0);
            if (strcmp(ts_node_type(identifier), "identifier") == 0) {
                size_t start = ts_node_start_byte(identifier);
                size_t end = ts_node_end_byte(identifier);
                char *original_name = strndup(source_code + start, end - start);

                char new_name[32];
                snprintf(new_name, sizeof(new_name), "v%zu", var_count++);

                add_variable(current_scope, original_name, new_name);

                StringChanges *c = malloc(sizeof(StringChanges));
                c->start = start;
                c->end = end;
                strncpy(c->text, new_name, sizeof(c->text));
                arrput(*changes, c);

                free(original_name);
            }
        }
    } else if (strcmp(node_type, "identifier") == 0) {
        TSNode parent = ts_node_parent(node);
        if (strcmp(ts_node_type(parent), "variable_declarator") == 0 &&
            ts_node_named_child(parent, 0).id == node.id) {
			// ...
        } else {
            size_t start = ts_node_start_byte(node);
            size_t end = ts_node_end_byte(node);
            char *name = strndup(source_code + start, end - start);

            const char *new_name = find_variable(current_scope, name);
            if (new_name != NULL) {
                StringChanges *c = malloc(sizeof(StringChanges));
                c->start = start;
                c->end = end;
                strncpy(c->text, new_name, sizeof(c->text));
                arrput(*changes, c);
            }
            free(name);
        }
    }

    uint32_t child_count = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_named_child(node, i);
        rename_variables(child, source_code, changes, current_scope);
    }

    if (new_scope != NULL) {
        Variable *var = new_scope->variables;
        while (var != NULL) {
            Variable *next = var->next;
            free(var->original_name);
            free(var->new_name);
            free(var);
            var = next;
        }
        free(new_scope);
    }
}

void rename_children_variables(TSNode root, const char *source_code, StringChanges ***changes) {
    Scope *global_scope = create_scope(NULL);
    rename_variables(root, source_code, changes, global_scope);

    Variable *var = global_scope->variables;
    while (var != NULL) {
        Variable *next = var->next;
        free(var->original_name);
        free(var->new_name);
        free(var);
        var = next;
    }
    free(global_scope);
}

int compare_size_t_desc(const void *a, const void *b) {
    return (*(size_t *)b - *(size_t *)a);
}

int bpe_parse(char *path) {
	var_count = 0;
	printf("[INFO] Processing %s file.\n", path);

	//
	// Loading javascript file.
	//

	FILE *file = fopen(path, "r");
	if(file == NULL) {
		fprintf(stderr, "[ERROR] Failed to open the file.");
		return 1;
	}

	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *input_stream = (char *)malloc(file_size + 1);
	if (input_stream == NULL) {
		fprintf(stderr, "[ERROR] Failed to allocate memory for input stream.");
		fclose(file);
		return 1;
	}

	size_t read_size = fread(input_stream, 1, file_size, file);
	if (read_size != file_size) {
		fprintf(stderr, "[ERROR] Failed to read the file.");
		free(input_stream);
		fclose(file);
		return 1;
	}
	fclose(file);
	input_stream[file_size] = '\0';

	//
	// Rename variables using tree-sitter
	//
    TSParser *parser = ts_parser_new();
	ts_parser_set_language(parser, tree_sitter_javascript());

	TSTree *tree = ts_parser_parse_string(parser, NULL, input_stream, strlen(input_stream));
    TSNode root = ts_tree_root_node(tree);

	printf("[INFO] Code parsed using tree-sitter.\n");

	StringChanges **changes = NULL;
	rename_children_variables(root, input_stream, &changes);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

	//
	// Update the variable name in code.
	//
	char *code_output = (char *)malloc(file_size + 1);
	if (code_output == NULL) {
		fprintf(stderr, "[ERROR] Failed to allocate memory for code output.");

		for(size_t i = 0; i < arrlenu(changes); ++i)
			free(changes[i]);
		arrfree(changes);
		free(input_stream);

		return 1;
	}

	size_t out_index = 0, src_index = 0;
	for(size_t i = 0; i < arrlenu(changes); ++i) {
		/*
		printf("%.*s => %s\n",
			   (int)(changes[i]->end - changes[i]->start),
			   input_stream + changes[i]->start,
			   changes[i]->text);
		*/

		while (src_index < changes[i]->start)
			code_output[out_index++] = input_stream[src_index++];

		const char *r = changes[i]->text;
		while (*r)
			code_output[out_index++] = *r++;

		src_index = changes[i]->end;
		free(changes[i]);
	}

	while (input_stream[src_index])
        code_output[out_index++] = input_stream[src_index++];
    code_output[out_index] = '\0';

	// printf("%s", code_output);
	printf("[INFO] Renamed %zu variables in the code.\n", arrlenu(changes));

	arrfree(changes);
	free(input_stream);

	//
	// BPE logic
	//
	char *code_output_end = code_output + out_index;
	Item **items = NULL;

	stb_lexer lexer;
	char string_store[1028];
	stb_c_lexer_init(&lexer, code_output, code_output_end, string_store, sizeof(string_store));

	int token = stb_c_lexer_get_token(&lexer);
	while(token != 0) {
		if(lexer.token == CLEX_parse_error) {
			fprintf(stderr, "[ERROR] Parse error.\n");
		} else {
			Item *itm = malloc(sizeof(Item));
			itm->left = NULL;
			itm->right = NULL;
			itm->value.string = NULL;
			copy_into_item(itm, &lexer);
			arrput(items, itm);
			arrput(global_items, itm);
		}
		token = stb_c_lexer_get_token(&lexer);
	}

	if(arrlenu(items) < 2) {
		fprintf(stderr, "[ERROR] Not enough tokens.\n");
		for(size_t i = 0; i < arrlenu(items); ++i)
			free_item(items[i]);
		arrfree(items);
		free(code_output);
		return 1;
	}

	Pair *merged_pairs = NULL;
	size_t *match_indexes = NULL;

	while(1) {
		size_t m_freq = 2, freq = 0, p1 = 0, p2 = 1, match_count = 0;
		while(p2 < arrlenu(items)) {
			//
			// NOTE: Each pair will be freed by bpe_free()
			//
			arrsetlen(match_indexes, 0);

			//
			// Sliding window
			//
			for (size_t k = 0; k + 1 < arrlenu(items); ++k) {
				if (items_equal(items[k], items[p1]) && items_equal(items[k+1], items[p2])) {
					arrput(match_indexes, k);
					++freq;
				}
			}

			if(freq > m_freq && pair_already_merged(merged_pairs, items[p1], items[p2]) < 0) {
				int global_ix = pair_already_merged(merged_pairs, items[p1], items[p2]);
				size_t item_id;

				if(global_ix > -1) {
					item_id = global_pairs[global_ix]->item_id;
				} else {
					item_id = item_counter++;
				}

				Pair *p = malloc(sizeof(Pair));
				p->a = items[p1];
				p->b = items[p2];
				p->item_id = item_id;

				arrput(merged_pairs, *p);
				arrput(global_pairs, p);

				// Process match_indexes in reverse order to avoid index
				// invalidation
				qsort(match_indexes, arrlenu(match_indexes), sizeof(size_t), compare_size_t_desc);

				for(size_t i = 0; i < arrlenu(match_indexes); ++i) {
					size_t k = match_indexes[i];
					// if(i > 0) --k;

					assert(k + 1 < arrlenu(items));

					Item *itm = malloc(sizeof(Item));
					itm->id = item_id;
					itm->left = items[k];
					itm->right = items[k+1];
					itm->value.string = NULL;
					arrput(global_items, itm);

					items[k] = itm;

					arrdel(items, k + 1);
				}

				match_count++;
				m_freq = freq;
			}
			++p1;
			++p2;
			freq = 0;
		}
		if(match_count < 1) break;
	}
	arrfree(match_indexes);
	arrfree(merged_pairs);

	//
	// NOTE: Each item will be freed by bpe_free()
	//
	arrfree(items);
	free(code_output);

	printf("[INFO] File processing done.\n");

	return 0;
}

void bpe_print(size_t print_all) {
	if(print_all > 0) {
		for(size_t i = 0; i < arrlenu(global_pairs); ++i) {
			print_item(global_pairs[i]->a);
			printf(" and ");
			print_item(global_pairs[i]->b);
			printf("\n");
		}
	}
	printf("\n Totally %zu pairs\n", arrlenu(global_pairs));
}

void bpe_save(const char *path) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "[ERROR] Failed to open file for saving: %s\n", path);
        return;
    }

    size_t pair_count = arrlenu(global_pairs);
    fwrite(&pair_count, sizeof(size_t), 1, file);

    for (size_t i = 0; i < pair_count; ++i) {
        Pair *pair = global_pairs[i];

        fwrite(&pair->item_id, sizeof(size_t), 1, file);

        for (int j = 0; j < 2; ++j) {
            Item *item = (j == 0) ? pair->a : pair->b;
            fwrite(&item->id, sizeof(size_t), 1, file);
            fwrite(&item->value.token, sizeof(int), 1, file);

            switch (item->value.token) {
			case CLEX_id:
			case CLEX_dqstring:
			case CLEX_sqstring: {
				size_t str_len = strlen(item->value.string) + 1;
				fwrite(&str_len, sizeof(size_t), 1, file);
				fwrite(item->value.string, sizeof(char), str_len, file);
				break;
			}
			case CLEX_intlit:
				fwrite(&item->value.int_number, sizeof(int64_t), 1, file);
				break;
			case CLEX_floatlit:
				fwrite(&item->value.real_number, sizeof(double), 1, file);
				break;
			default:
				break;
            }
        }
    }

    fclose(file);
    printf("[INFO] Saved %zu pairs to %s\n", pair_count, path);
}

void bpe_load(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "[ERROR] Failed to open file for loading: %s\n", path);
        return;
    }

    size_t pair_count;
    fread(&pair_count, sizeof(size_t), 1, file);

    for (size_t i = 0; i < pair_count; ++i) {
        Pair *pair = malloc(sizeof(Pair));

        fread(&pair->item_id, sizeof(size_t), 1, file);

        for (int j = 0; j < 2; ++j) {
            Item *item = malloc(sizeof(Item));
            fread(&item->id, sizeof(size_t), 1, file);
            fread(&item->value.token, sizeof(int), 1, file);

            switch (item->value.token) {
			case CLEX_id:
			case CLEX_dqstring:
			case CLEX_sqstring: {
				size_t str_len;
				fread(&str_len, sizeof(size_t), 1, file);
				item->value.string = malloc(str_len);
				fread(item->value.string, sizeof(char), str_len, file);
				break;
			}
			case CLEX_intlit:
				fread(&item->value.int_number, sizeof(int64_t), 1, file);
				break;
			case CLEX_floatlit:
				fread(&item->value.real_number, sizeof(double), 1, file);
				break;
			default:
				item->value.string = NULL;
				break;
            }

            if (j == 0) {
                pair->a = item;
            } else {
                pair->b = item;
            }
        }

        arrput(global_pairs, pair);
    }

    fclose(file);
    printf("[INFO] Loaded %zu pairs from %s\n", pair_count, path);
}

void bpe_free() {
	for(size_t i = 0; i < arrlenu(global_items); ++i)
		free_item(global_items[i]);
	arrfree(global_items);

	printf("[INFO] Clean up all global items.\n");

	for(size_t i = 0; i < arrlenu(global_pairs); ++i)
		free(global_pairs[i]);
	arrfree(global_pairs);

	printf("[INFO] Clean up all global pairs.\n");
}

size_t bpe_test(char *input) {
	size_t input_len = strlen(input);
	Item **items = NULL;

	stb_lexer lexer;
	char string_store[1028];
	stb_c_lexer_init(&lexer, input, input + input_len, string_store, sizeof(string_store));

	int token = stb_c_lexer_get_token(&lexer);
	while(token != 0) {
		if(lexer.token == CLEX_parse_error) {
			fprintf(stderr, "[ERROR] Parse error.\n");
			for(size_t i = 0; i < arrlenu(items); ++i)
				free_item(items[i]);
			arrfree(items);
			break;
		} else {
			Item *itm = malloc(sizeof(Item));
			itm->left = NULL;
			itm->right = NULL;
			itm->value.string = NULL;
			copy_into_item(itm, &lexer);
			arrput(items, itm);
		}
		token = stb_c_lexer_get_token(&lexer);
	}

	if(arrlenu(items) < 2) {
		fprintf(stderr, "[ERROR] Not enough tokens.\n");
		for(size_t i = 0; i < arrlenu(items); ++i)
			free_item(items[i]);
		arrfree(items);
		return 1;
	}

	//
	// Find next token based on last token.
	//
	Item *last_item = items[arrlenu(items) - 1];
	for(size_t i = 0; i < arrlenu(global_pairs); ++i) {
		if(items_equal(last_item, global_pairs[i]->a)) {
			print_item(global_pairs[i]->b);
			break;
		}
	}

	//
	// Cleanup
	//
	for(size_t i = 0; i < arrlenu(items); ++i)
		free_item(items[i]);
	arrfree(items);
	return 0;
}
