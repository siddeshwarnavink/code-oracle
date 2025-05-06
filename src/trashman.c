#include <stdio.h>
#include <string.h>

#define STB_C_LEXER_IMPLEMENTATION
// #define STB_DS_IMPLEMENTATION
#include "stb_c_lexer.h"
// #include "stb_ds.h"

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

int is_keyword(const char *str) {
    for (size_t i = 0; i < NUM_KEYWORDS; i++) {
        if (strcmp(str, keywords[i]) == 0) {
            return 1;
        }
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

	int token = stb_c_lexer_get_token(&lexer);
	while(token != 0) {
		switch(lexer.token) {
		case CLEX_parse_error:
			fprintf(stderr, "[ERROR] Parse error.\n");
			break;
		case CLEX_id:
			if(is_keyword(lexer.string)) {
				printf("keyword: %s\n", lexer.string);
			} else {
				printf("identifyer: %s\n", lexer.string);
			}
			break;
		case CLEX_dqstring:
			printf("string: %s\n", lexer.string);
			break;
		case CLEX_noteq:
			printf("not equal to\n");
			break;
		default:
			if(lexer.token < 256) {
				printf("%c\n", lexer.token);
			} else {
				printf("Unknown: %ld\n", lexer.token);
			}
			break;
		}
		token = stb_c_lexer_get_token(&lexer);
	}

	return 0;
}
