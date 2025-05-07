#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_C_LEXER_IMPLEMENTATION
#define STB_DS_IMPLEMENTATION
#include "stb_c_lexer.h"
#include "stb_ds.h"

typedef struct Item {
	size_t id;
	struct Item *left;
	struct Item *right;
	stb_lexer value;
	size_t free_visited;
} Item;

const char *keywords[] = {
    "React", "Component", "useState", "useEffect", "useContext", "useReducer", "useRef",
    "useMemo", "useCallback", "useLayoutEffect", "useImperativeHandle",
    "return", "render", "props", "children", "className", "onClick", "onSubmit", "onChange",
    "onInput", "onKeyDown", "onKeyUp", "onMouseEnter", "onMouseLeave", "onFocus",
    "onBlur", "onScroll", "style", "key", "ref",
    "const", "let", "var", "function", "class", "extends", "constructor", "if", "else",
    "switch", "case", "default", "break", "continue", "try", "catch", "finally",
    "throw", "return", "typeof", "instanceof", "new", "this", "super",
    "import", "from", "export", "default", "async", "await",
    "componentDidMount", "componentDidUpdate", "componentWillUnmount",
    "shouldComponentUpdate", "getDerivedStateFromProps", "getSnapshotBeforeUpdate",
    "setState", "state", "context", "useContext", "Provider", "Consumer",
    "div", "span", "input", "button", "form", "label", "ul", "li", "table", "thead",
    "tbody", "tr", "td", "th", "p", "h1", "h2", "h3", "h4", "h5", "h6", "a", "img",
    "href", "src", "alt", "title", "type", "value", "id", "name", "placeholder"
};
#define NUM_KEYWORDS (sizeof(keywords) / sizeof(keywords[0]))

size_t is_keyword(const char *str) {
    for (size_t i = 0; i < NUM_KEYWORDS; i++) {
        if (strcmp(str, keywords[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

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
	// Case: a a
	if(a->left == NULL && b->left == NULL
	   && a->right == NULL && b->right == NULL) {
		return tokens_equal(a->value, b->value);
	}

	// Case: X X
	if(a->left != NULL && b->left != NULL
	   && a->right != NULL && b->right != NULL) {
		size_t r1 = items_equal(a->left, b->left);
		size_t r2 = items_equal(a->right, b->right);
		return r1 && r2;
	}

	// Case: X a
	if(a->left != NULL && b->left == NULL) {
		return items_equal(a->left, b);
	}

	// Case: a X
	if(a->left == NULL && b->left != NULL) {
		return items_equal(a, b->left);
	}

	return 0;
}

void copy_into_item(Item *a, stb_lexer *b) {
	a->value.token = b->token;
	switch(b->token) {
	case CLEX_id:
	case CLEX_dqstring:
	case CLEX_sqstring:
		a->value.string =  malloc(strlen(b->string));
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
		if(itm->free_visited) return;
		itm->free_visited = 1;
		free_item(itm->left);
		free_item(itm->right);
		if(itm->value.string != NULL)
			free(itm->value.string);
		free(itm);
	}
}

void render_dot(Item *itm) {
	printf("    %zu;\n", itm->id);
	if (itm->left != NULL) {
		printf("    %zu -> %zu;\n", itm->id, itm->left->id);
	}
	if (itm->right != NULL) {
		printf("    %zu -> %zu;\n", itm->id, itm->right->id);
	}
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

	size_t token_id = 0;
	int token = stb_c_lexer_get_token(&lexer);
	while(token != 0) {
		if(lexer.token == CLEX_parse_error) {
			fprintf(stderr, "[ERROR] Parse error.\n");
		} else {
			Item *itm = malloc(sizeof(Item));
			itm->id = token_id++;
			itm->left = NULL;
			itm->right = NULL;
			itm->value.string = NULL;
			itm->free_visited = 0;
			copy_into_item(itm, &lexer);
			arrput(items, itm);
		}
		token = stb_c_lexer_get_token(&lexer);
	}

	if(arrlenu(items) < 2) {
		fprintf(stderr, "[ERROR] Not enough tokens.\n");
		return 1;
	}

	size_t p1 = 0, p2 = 1; // pair

	while(p2 < arrlenu(items)) {
		size_t i = p1 + 2, j = p2 + 2;
		Item *root = NULL;

		while(j < arrlenu(items)) {
			if(root == NULL && items_equal(items[i], items[p1]) && items_equal(items[j], items[p2])) {
				// From:
				//   X a b c X a d b a a c d
				//   ^ ^     ^ ^
				//   p1p2    i j
				//
				// To:
				//   Y b c Y d b a a c d
				//   ^       ^ ^
				//   p1      i j
				//   p2

				root = malloc(sizeof(Item));
				root->id = token_id++;
				root->left = items[p1];
				root->right = items[p2];
				root->value.string = NULL;
				root->free_visited = 0;

				items[p1] = root;
				arrdel(items, p2);
				p2 = p1;

				if(items[i]->left == NULL)
					free_item(items[i]);
				items[i] = root;
				if(items[j]->left == NULL)
					free_item(items[j]);
				arrdel(items, j);
			} else if (root != NULL && items_equal(root->left, items[i]) && items_equal(root->right, items[j])) {
				// From:
				//   X a b c a a d b a a c d
				//   ^       ^ ^
				//   p1      i j
				//   p2
				//
				// To:
				//   X a b c X d b a a c d
				//   ^         ^ ^
				//   p1        i j
				//   p2

				if(items[i]->left == NULL)
					free_item(items[i]);
				items[i] = root;
				if(items[j]->left == NULL)
					free_item(items[j]);
				arrdel(items, j);

				++i;
				++j;
			} else {
				i += 2;
				j += 2;
			}
		}
		if(root == NULL) {
			++p1;
		}
		++p2;
	}

	printf("digraph G {\n");
	for(size_t i = 0; i < arrlenu(items); ++i) {
		render_dot(items[i]);
		free_item(items[i]);
	}
	printf("}\n");
	arrfree(items);
	free(input_stream);

	return 0;
}
