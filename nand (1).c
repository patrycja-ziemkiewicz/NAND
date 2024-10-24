#include "nand.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>

struct nand {
    bool output_value;              // Wartość wyjściowa bramki NAND
    bool if_signal;                 // Flaga określająca, czy bramka reprezentuje sygnał (czyli nie ma wejść)
    bool const* signal;             // Wskaźnik na wartość sygnału (jeśli if_signal == true)
    unsigned number_of_outputs;     // Ilość wyjść bramki NAND
    unsigned number_of_inputs;      // Ilość wejść bramki NAND
    unsigned allocated_memory_outputs; // Liczba zaalokowanej pamięci dla wyjść
    unsigned evaluete_id;           // Identyfikator używany podczas oceny ścieżki krytycznej
    ssize_t critical_path;          // Długość ścieżki krytycznej
    struct nand** output;           // Tablica wskaźników na wyjścia bramki NAND
    struct nand** input;            // Tablica wskaźników na wejścia bramki NAND
};
typedef struct nand nand_t;

unsigned id = 0; // Zmienna globalna pomocnicza dla określenia identyfikatora oceny bramki NAND

// Funkcja tworząca nową bramkę NAND
nand_t* nand_new(unsigned n) {
    nand_t* g = (nand_t*)malloc(sizeof(nand_t));
    if (g == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    nand_t** temp = NULL;
    if (n != 0) temp = (nand_t**)malloc(n * sizeof(nand_t*));
    if (n != 0 && temp == NULL) {
        errno = ENOMEM;
        free(g);
        return NULL;
    }
    for (unsigned i = 0; i < n; i++) {
        temp[i] = NULL;
    }
    g->number_of_inputs = n;
    g->number_of_outputs = 0;
    g->evaluete_id = -1;
    g->critical_path = 0;
    g->allocated_memory_outputs = 0;
    g->output = NULL;
    g->input = temp;
    g->output_value = false;
    g->if_signal = false;
    g->signal = NULL;
    return g;
}

// Funkcja tworząca sygnał
static nand_t* create_signal(bool const *value) {
    nand_t* signal = nand_new(0);
    if (signal != NULL) {
        signal->if_signal = true;
        signal->signal = value;
    }
    return signal;
}

// Funkcja rozłączająca połączenia z bramką NAND
// Bool input_output określa czy rozlaczamy bramki w relacji input-output czy output-input
static void nand_disconnect(nand_t *deleted_nand, nand_t **tab, unsigned n, bool input_output, nand_t *g) {
    for (unsigned i = 0; i < n; i++) {
        if (deleted_nand == tab[i]) {
            if (input_output) {
                g->number_of_outputs--;
                if (g->number_of_outputs == 0) {
                    g->allocated_memory_outputs = 0;
                    free(g->output);
                    g->output = NULL;
                }
                else {
                    g->output[i] = g->output[n - 1];
                    if (g->number_of_outputs * 2 + 1 <= g->allocated_memory_outputs) {
                        g->allocated_memory_outputs = g->allocated_memory_outputs / 2;
                        nand_t** temp = g->output;
                        temp = (nand_t**) realloc(temp, g->allocated_memory_outputs * sizeof(nand_t*));
                        if (temp == NULL) {
                            errno = ENOMEM;
                            return;
                        }
                        g->output = temp;
                    }
                }
            }
            else {
                g->input[i] = NULL;
            }
            return;
        }
    }
}

// Funkcja usuwająca bramkę NAND
void nand_delete(nand_t *g) {
    if (g != NULL) {
        for (unsigned i = 0; i < g->number_of_inputs; i++) {
            if (g->input[i] != NULL) {
                if (g->input[i]->if_signal) {
                    free(g->input[i]);
                }
                else {
                    nand_disconnect(g, g->input[i]->output, g->input[i]->number_of_outputs, true, g->input[i]);
                    if (errno == ENOMEM)
                        return;
                }
            }
        }
        if (g->input != NULL) free(g->input);
        for (unsigned i = 0; i < g->number_of_outputs; i++) {
            if (g->output[i] != NULL) {
                nand_disconnect(g, g->output[i]->input, g->output[i]->number_of_inputs, false, g->output[i]);
            }
        }
        if (g->output != NULL) free(g->output);
        free(g);
    }
}

// Funkcja alokujaca pamiec na tablice outputów bramki NAND
static void allocate_new_memory(unsigned* n, nand_t*** tab) {
    *n = (*n * 2) + 1;
    nand_t** temp = *tab;
    if (temp != NULL)
        temp = (nand_t**)realloc(temp, *n * sizeof(nand_t*));
    else
        temp = (nand_t**)malloc(*n * sizeof(nand_t*));
    *tab = temp;
}

// Funkcja łącząca dwie bramki nand
int nand_connect_nand(nand_t *g_out, nand_t *g_in, unsigned k) {
    if (g_out == NULL || g_in == NULL || k >= g_in->number_of_inputs) {
        errno = EINVAL;
        return -1;
    }
    if (g_in->input[k] == g_out) return 0;
    if (!g_out->if_signal) {
        if (g_out->allocated_memory_outputs <= g_out->number_of_outputs + 1) {
            allocate_new_memory(&g_out->allocated_memory_outputs, &g_out->output);
            if (g_out->output == NULL) {
                errno = ENOMEM;
                return -1;
            }
        }
        g_out->output[g_out->number_of_outputs] = g_in;
        g_out->number_of_outputs++;
    }
    if (g_in->input[k] != NULL) {
        if (g_in->input[k]->if_signal) {
            free(g_in->input[k]);
        }
        else {
            nand_disconnect(g_in, g_in->input[k]->output, g_in->input[k]->number_of_outputs, true, g_in->input[k]);
            if (errno == ENOMEM)
                return -1;
        }
    }
    g_in->input[k] = g_out;
    return 0;
}

// Funkcja łącząca bramke nand z sygnałem
int nand_connect_signal(bool const *s, nand_t *g, unsigned k) {
    if (s == NULL || g == NULL || k >= g->number_of_inputs) {
        errno = EINVAL;
        return -1;
    }
    nand_t*  signal = create_signal(s);
    if (signal == NULL) {
        errno = ENOMEM;
        return -1;
    }
    return nand_connect_nand(signal, g, k);
}

static ssize_t max(ssize_t a, ssize_t b) {
    return (a > b ? a : b);
}

// Funkcja obliczająca ścieżkę krytyczną dla bramki nand
static ssize_t critical_path(nand_t *g) {
    if (g->if_signal) {
        if (g->signal != NULL) {
            g->output_value = *g->signal;
            return 0;
        }
        else {
            errno = ECANCELED;
            return -1;
        }
    }
    else {
        ssize_t temp_max = 0;
        g->output_value = false;
        for (unsigned i  = 0; i < g->number_of_inputs; i++) {
            if (g->input[i] != NULL) {
                if (g->input[i]->evaluete_id == id) {
                    // Sprawdzam czy układ bramek nie jest zacyklony
                    if (g->input[i]->critical_path == -1) {
                        errno = ECANCELED;
                        return -1;
                    }
                    temp_max = max(temp_max, g->input[i]->critical_path);
                }
                else {
                    g->input[i]->evaluete_id = id;
                    g->input[i]->critical_path = -1;
                    ssize_t zmienna =critical_path(g->input[i]);
                    g->input[i]->critical_path = zmienna + 1;
                    if (zmienna == -1) return -1;
                    temp_max = max(temp_max, g->input[i]->critical_path);
                }
                if (!g->input[i]->output_value) g->output_value = true;
            }
            else {
                errno = ECANCELED;
                return -1;
            }
        }
        return temp_max;
    }
}

// Funkcja wyznaczająca maksymalną ściężkę krytyczną dla tablicy bramek NAND
ssize_t nand_evaluate(nand_t **g, bool *s, size_t m) {
    if (m <= 0 || g == NULL || s == NULL) {
        errno = EINVAL;
        return -1;
    }
    ssize_t temp_max = 0;
    for (size_t i = 0; i < m; i++) {
        if (g[i] == NULL) {
            errno = EINVAL;
            return -1;
        }
    }
    id = id + g[0]->evaluete_id + 1;
    for (size_t i = 0; i < m; i++) {
        ssize_t critical = critical_path(g[i]);
        temp_max = max(temp_max, critical);
        if (critical == -1) return -1;
        s[i] = g[i]->output_value;
    }
    id++;
    return temp_max;
}

// Funkcja zwracająca liczbę wyjść bramki NAND
ssize_t nand_fan_out(nand_t const *g) {
    if (g == NULL) {
        errno = EINVAL;
        return -1;
    }
    return g->number_of_outputs;
}

void*   nand_input(nand_t const *g, unsigned k) {
    if (g == NULL || k >= g->number_of_inputs) {
        errno = EINVAL;
        return NULL;
    }
    if (g->input[k] == NULL) {
        errno = 0;
        return NULL;
    }
    else if (g->input[k]->if_signal) {
        return (bool*) g->input[k]->signal;
    }
    else {
        return g->input[k];
    }
}

nand_t* nand_output(nand_t const *g, ssize_t k) {
    if (g == NULL || g->number_of_outputs <= k) {
        errno = EINVAL;
        return NULL;
    }
    return g->output[k];
}

