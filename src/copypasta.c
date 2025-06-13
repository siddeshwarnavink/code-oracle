#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <curl/curl.h>

typedef struct Memory {
    char *data;
    size_t size;
} Memory;

int gfg_extract_javascript(char *html_code, char *path) {
    if (html_code == NULL) {
        fprintf(stderr, "[ERROR] No HTML code provided.\n");
        return 1;
    }
    
    const char *start_tag = "<code class=\"language-javascript\">";
    const char *end_tag = "</code>";

    char *start = strstr(html_code, start_tag);
    char *buf = NULL;

    if(start) {
        start += strlen(start_tag);
        const char *end = strstr(start, end_tag);

        if (end) {
            buf = malloc((end - start + 1) * sizeof(char));
            if (buf == NULL) {
                fprintf(stderr, "[ERROR] Not enough memory to store buffer.\n");
            }
            char expr_buf[5];

            size_t i = 0, j = 0;
            bool ignore = false, expr = false;
            for (const char *ptr = start; ptr < end; ++ptr) {
                switch(*ptr) {
                case '<':
                    ignore = true;
                    break;
                case '>':
                    ignore = false;
                    break;
                case '&':
                    expr = true;
                    break;
                case ';':
                    if(expr) {
                        expr_buf[j] = '\0';
                        if(strcmp(expr_buf, "lt") == 0) {
                            buf[i++] = '<';
                        } else if(strcmp(expr_buf, "gt") == 0) {
                            buf[i++] = '>';
                        } else if(strcmp(expr_buf, "quot") == 0) {
                            buf[i++] = '"';
                        } else if(strcmp(expr_buf, "amp") == 0) {
                            buf[i++] = '&';
                        } else if(strcmp(expr_buf, "#39") == 0) {
                            buf[i++] = '`';
                        } else {
                            fprintf(stderr, "[ERROR] Unknown HTML expression: %s\n", expr_buf);
                            free(buf);
                            return 1;
                        }
                        j = 0;
                        expr = false;
                        break;
                    }
                default:
                    if(expr)
                        expr_buf[j++] = *ptr;
                    else if(!ignore)
                        buf[i++] = *ptr;
                    break;
                }
            }

            buf[i] = '\0';

            FILE *file = fopen(path, "w");
            if(file == NULL) {
                fprintf(stderr, "[ERROR] Failed to write output to file\n");
                free(buf);
                return 1;
            }

            fprintf(file, "%s", buf);
            fclose(file);

            printf("[INFO] Saved to file %s\n", path);

            free(buf);
        } else {
            fprintf(stderr, "[ERROR] No end tag found.\n");
            return 1;
        }
    } else {
        fprintf(stderr, "[ERROR] No start tag found.\n");
        return 1;
    }
    return 0;
}

int gfg_extract_table_links(char *html_code, char *path) {
    const char *start_tag = "<table>";
    const char *end_tag = "</table>";

    char *start = strstr(html_code, start_tag);
    char *buf = NULL;

    if(start) {
        start += strlen(start_tag);
        const char *end = strstr(start, end_tag);

        if (end) {
            buf = malloc((end - start + 1) * sizeof(char));
            if (buf == NULL) {
                fprintf(stderr, "[ERROR] Not enough memory to store buffer.\n");
            }

            size_t i = 0, j;
            bool reading_url = false;
            for (const char *ptr = start; ptr < end; ++ptr) {
                switch(*ptr) {
                case '"':
                    if(reading_url) {
                        if(buf[i - 1] == '/') {
                            buf[i++] = '\n';
                        } else {
                            i = j;
                        }
                    } else {
                        j = i;
                    }
                    reading_url = !reading_url;
                    break;
                default:
                    if(reading_url)
                        buf[i++] = *ptr;
                    break;
                }
            }

            buf[i] = '\0';

            FILE *file = fopen(path, "w");
            if(file == NULL) {
                fprintf(stderr, "[ERROR] Failed to write output to file\n");
                free(buf);
                return 1;
            }

            fprintf(file, "%s", buf);
            fclose(file);

            printf("[INFO] Table links saved to file %s\n", path);

            free(buf);
        } else {
            fprintf(stderr, "[ERROR] No table end tag found.\n");
            return 1;
        }
    } else {
        fprintf(stderr, "[ERROR] No table start tag found.\n");
        return 1;
    }
    return 0;
}

size_t gfg_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    Memory *mem = (Memory *)userp;

    char *ptr = realloc(mem->data, mem->size + total_size + 1);
    if (ptr == NULL) {
        fprintf(stderr, "[ERROR] Not enough memory to store HTML content\n");
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, total_size);
    mem->size += total_size;
    mem->data[mem->size] = '\0';

    return total_size;
}

int gfg_table_links(char *url, char *path) {
    CURL *curl;
    CURLcode res;
    Memory chunk = {0};

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gfg_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "[ERROR] cURL error: %s\n", curl_easy_strerror(res));
        } else {
            return gfg_extract_table_links(chunk.data, path);
        }

        curl_easy_cleanup(curl);
        free(chunk.data);
    } else {
        fprintf(stderr, "[ERROR] Failed to initialize libcurl\n");
    }
    return 0;
}

int gfg_scrape(char *url, char *path) {
    CURL *curl;
    CURLcode res;
    Memory chunk = {0};

    printf("[INFO] Web scraping code from \"%s\"\n", url);

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gfg_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "[ERROR] cURL error: %s\n", curl_easy_strerror(res));
        } else {
            return gfg_extract_javascript(chunk.data, path);
        }

        curl_easy_cleanup(curl);
        free(chunk.data);
    } else {
        fprintf(stderr, "[ERROR] Failed to initialize libcurl\n");
        return 1;
    }
	return 0;
}
