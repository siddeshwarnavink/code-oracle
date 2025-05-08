#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define STB_C_LEXER_IMPLEMENTATION
#define STB_DS_IMPLEMENTATION
#include "stb_c_lexer.h"
#include "stb_ds.h"

typedef struct Item {
	size_t id;
	struct Item *left;
	struct Item *right;
	stb_lexer value;
} Item;

typedef struct Pair {
	Item *a;
	Item *b;
} Pair;

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

void free_item(Item *itm) {
	if(itm != NULL) {
		free_item(itm->left);
		free_item(itm->right);
		if(itm->value.string != NULL)
			free(itm->value.string);
		free(itm);
	}
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

size_t pair_already_merged(Pair *pairs, Item *a, Item *b) {
	if(pairs == NULL || a == NULL || b == NULL)
		assert(1);

	if(arrlenu(pairs) == 0)
		assert(1);

	for(size_t i = 0; i < arrlenu(pairs); ++i) {
		if((items_equal(pairs[i].a, a) && items_equal(pairs[i].b, b))
		   || (items_equal(pairs[i].a, b) && items_equal(pairs[i].b, a)))
			return 1;
	}
	return 0;
}

int parse_code(char *path) {
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

	char *input_stream_end = input_stream + file_size;

	stb_lexer lexer;
	char string_store[1028];
	stb_c_lexer_init(&lexer, input_stream, input_stream_end, string_store, sizeof(string_store));

	Item **items = NULL;
	size_t item_counter = 0;

	int token = stb_c_lexer_get_token(&lexer);
	while(token != 0) {
		if(lexer.token == CLEX_parse_error) {
			fprintf(stderr, "[ERROR] Parse error.\n");
		} else {
			Item *itm = malloc(sizeof(Item));
			itm->id = item_counter++;
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
		free(input_stream);
		return 1;
	}

	Pair *merged_pairs = NULL;
	size_t *match_indexes = NULL;

	while(1) {
		size_t m_freq = 2, freq = 0, p1 = 0, p2 = 1, match_count = 0;
		while(p2 < arrlenu(items)) {
			arrsetlen(match_indexes, 0);
			for (size_t k = 0; k + 1 < arrlenu(items); ++k) {
				if (items_equal(items[k], items[p1]) && items_equal(items[k+1], items[p2])) {
					arrput(match_indexes, k);
					++freq;
				}
			}
			if(freq > m_freq && !pair_already_merged(merged_pairs, items[p1], items[p2])) {
				size_t item_id = item_counter++;
				print_item(items[p1]);
				printf(" and ");
				print_item(items[p2]);
				printf("\n");

				Pair p = { items[p1], items[p2] };
				arrput(merged_pairs, p);

				for(size_t i = 0; i < arrlenu(match_indexes); ++i) {
					size_t k = match_indexes[i];
					if(i > 0) --k;

					Item *itm = malloc(sizeof(Item));
					itm->id = item_id;
					itm->left = items[k];
					itm->right = items[k+1];
					itm->value.string = NULL;

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

	for(size_t i = 0; i < arrlenu(items); ++i) {
		free_item(items[i]);
	}
	arrfree(items);
	free(input_stream);

	return 0;
}
