#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define STB_C_LEXER_IMPLEMENTATION
#define STB_DS_IMPLEMENTATION
#include "stb_c_lexer.h"
#include "stb_ds.h"
#include "tree_sitter/api.h"
#include "tree-sitter-javascript.h"

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

typedef struct StringChanges {
	size_t start;
	size_t end;
	char text[32];
} StringChanges;

size_t var_count = 0;

typedef struct Variable {
    char *original_name;
    char *new_name;
    struct Variable *next;
} Variable;

typedef struct Scope {
    Variable *variables;
    struct Scope *parent;
} Scope;

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

// Function to create a new scope
Scope* create_scope(Scope *parent) {
    Scope *scope = (Scope*)malloc(sizeof(Scope));
    scope->variables = NULL;
    scope->parent = parent;
    return scope;
}

// Function to add a variable to the current scope
void add_variable(Scope *scope, const char *original_name, const char *new_name) {
    Variable *var = (Variable*)malloc(sizeof(Variable));
    var->original_name = strdup(original_name);
    var->new_name = strdup(new_name);
    var->next = scope->variables;
    scope->variables = var;
}

// Function to look up a variable in the scope chain
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

// Helper function to check if a node type is a scope boundary
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
    
    // Enter new scope if current node is a scope boundary
    Scope *new_scope = NULL;
    if (is_scope_boundary(node_type)) {
        new_scope = create_scope(current_scope);
        current_scope = new_scope;
    }

    // Process variable declarations
    if (strcmp(node_type, "lexical_declaration") == 0 ||
        strcmp(node_type, "variable_declaration") == 0) {
        
        TSNode declarator = ts_node_named_child(node, 0);
        if (strcmp(ts_node_type(declarator), "variable_declarator") == 0) {
            TSNode identifier = ts_node_named_child(declarator, 0);
            if (strcmp(ts_node_type(identifier), "identifier") == 0) {
                // Get original variable name
                size_t start = ts_node_start_byte(identifier);
                size_t end = ts_node_end_byte(identifier);
                char *original_name = strndup(source_code + start, end - start);
                
                // Generate new name
                char new_name[32];
                snprintf(new_name, sizeof(new_name), "v%zu", var_count++);
                
                // Add to current scope
                add_variable(current_scope, original_name, new_name);
                
                // Record declaration change
                StringChanges *c = malloc(sizeof(StringChanges));
                c->start = start;
                c->end = end;
                strncpy(c->text, new_name, sizeof(c->text));
                arrput(*changes, c);
                
                free(original_name);
            }
        }
    }
    // Process identifier usages
    else if (strcmp(node_type, "identifier") == 0) {
        // Check if this is part of a declaration (already handled)
        TSNode parent = ts_node_parent(node);
        if (strcmp(ts_node_type(parent), "variable_declarator") == 0 &&
            ts_node_named_child(parent, 0).id == node.id) {
            // Skip declaration identifiers
        } else {
            // Look up variable in scope chain
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

    // Recursively process children
    uint32_t child_count = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_named_child(node, i);
        rename_variables(child, source_code, changes, current_scope);
    }

    // Clean up scope if we created a new one
    if (new_scope != NULL) {
        // Free variables in the scope
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

// Initialization function
void rename_children_variables(TSNode root, const char *source_code, StringChanges ***changes) {
    Scope *global_scope = create_scope(NULL);
    rename_variables(root, source_code, changes, global_scope);
    
    // Clean up global scope
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

/*
void rename_children_variables(TSNode root, StringChanges ***changes) {
    uint32_t count = ts_node_named_child_count(root);
    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_named_child(root, i);

		if (strcmp(ts_node_type(child), "class_declaration") == 0
			|| strcmp(ts_node_type(child), "class_body") == 0
			|| strcmp(ts_node_type(child), "method_definition") == 0
			|| strcmp(ts_node_type(child), "statement_block") == 0
			|| strcmp(ts_node_type(child), "if_statement") == 0
			) {
			rename_children_variables(child, changes);
		}
        else if (strcmp(ts_node_type(child), "lexical_declaration") == 0) {
            TSNode var_node = ts_node_named_child(child, 0);
			assert(strcmp(ts_node_type(var_node), "variable_declarator") == 0);

			TSNode ident_node = ts_node_named_child(var_node, 0);
			assert(strcmp(ts_node_type(ident_node), "identifier") == 0);

			size_t start = ts_node_start_byte(ident_node);
			size_t end = ts_node_end_byte(ident_node);

			StringChanges *c = malloc(sizeof(StringChanges));
			c->start = start;
			c->end = end;
			snprintf(c->text, sizeof(c->text), "v%zu", var_count++);
			arrput(*changes, c);
        }
		// else {
		// 	printf("%s\n", ts_node_type(child));
		// }
    }
}
*/

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

	//
	// Rename variables using tree-sitter
	//
    TSParser *parser = ts_parser_new();
	ts_parser_set_language(parser, tree_sitter_javascript());

	TSTree *tree = ts_parser_parse_string(parser, NULL, input_stream, strlen(input_stream));
    TSNode root = ts_tree_root_node(tree);

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

	printf("%s", code_output);

	arrfree(changes);
	free(input_stream);
	free(code_output);
	return 0;

	//
	// BPE logic
	//
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

			//
			// Sliding window
			//
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
