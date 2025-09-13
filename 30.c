#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_NOME 20
#define HASH_MAP_SIZE 307

typedef struct Lotto {
    int scadenza; // Istante di scadenza del lotto
    int quantita; // La quantità di prodotto nel lotto
    struct Lotto *next;  // Per la gestione delle collisioni (linked list)
} Lotto;

typedef struct Ingrediente {
    Lotto *lotti;   // Lista dei lotti di un ingrediente
    char nome[MAX_NOME];    // Il nome dell'ingrediente
    struct Ingrediente *next;  // Per la gestione delle collisioni (linked list)
} Ingrediente; // Nodo della linked list

typedef struct CatalogoIngredienti {
    Ingrediente *buckets[HASH_MAP_SIZE];  // Array di puntatori a lotti
} CatalogoIngredienti;

typedef struct Ingredienti_ricetta {
    Ingrediente *stock; // Puntatore all'ingrediente nel magazzino
    int quantita;    // Quantità necessaria per ingrediente
    struct Ingredienti_ricetta *next;
}Ingredienti_ricetta;

typedef struct Ricetta {
    char nome[MAX_NOME];    // Il nome della ricetta
    Ingredienti_ricetta *ingredienti_necessari; // Lista degli ingredienti necessari per la ricetta
    struct Ricetta *next;
} Ricetta;

typedef struct {
    Ricetta *ricette[HASH_MAP_SIZE]; // hash table delle ricette
} CatalogoRicette;

typedef struct {
    Ricetta *ricetta;   // Puntatore alla ricetta ordinata
    int istante_arrivo;   // L'istante in cui l'ordine è stato ricevuto
    char nome_ricetta[MAX_NOME]; // Il nome della ricetta ordinata
    int quantita;         // Il numero di dolci ordinati
} Ordine;

// Nodo della lista collegata che rappresenta un singolo ordine nella coda
typedef struct Nodo {
    int peso;   // Peso dell'ordine (quantità totale di ingredienti necessari)
    Ordine *ordine; // Puntatore all'ordine
    struct Nodo* prossimo;
} Nodo;

// ordini pronti, in attesa e ritirati
typedef struct {
    Nodo* fronte;  // Puntatore al nodo in fronte alla coda (primo elemento)
    Nodo* retro;   // Puntatore al nodo alla fine della coda (ultimo elemento)
} Coda;


CatalogoIngredienti* init_map();
CatalogoRicette* init_catalogo();
unsigned int hash(char str[MAX_NOME]);
void inserisci_lotto(FILE *file, CatalogoIngredienti *map);
void rimuovi_lotti(CatalogoIngredienti *map, CatalogoRicette *cat, Ordine *ordine, int istante_corrente);
Ricetta* trova_ricetta(CatalogoRicette *cat, char nome[MAX_NOME]);
void aggiungi_ricetta(FILE *file, CatalogoRicette *cat, CatalogoIngredienti *map);
void rimuovi_ricetta(FILE *file, CatalogoRicette *cat, Coda *ordini_in_attesa, Coda *ordini_pronti);
void free_catalogo(CatalogoRicette *cat, char nome_ricetta[MAX_NOME]);
int verifica_fattibilita(CatalogoIngredienti *map,CatalogoRicette *cat, Ordine *ordine, int istante_corrente);
void gestione_ordine(CatalogoIngredienti *map, CatalogoRicette *cat, Coda *ordini_pronti, Coda *ordini_in_attesa, Ordine *ordine, int istante_corrente);
void controllo_rifornimento(CatalogoIngredienti *map, CatalogoRicette *cat, Coda *ordini_pronti, Coda *ordini_in_attesa, int istante_corrente);
void ritiro(Coda *ordini_ritirati, Coda *ordini_pronti, int capacita, CatalogoRicette *cat);
int somma_quantita(CatalogoRicette *cat , Ordine *ordine);
Ordine *init_ordine(FILE *file, int istante_arrivo, CatalogoRicette *cat);
void enqueue_ritiro(CatalogoRicette *cat, Coda* coda, Ordine *nuovoOrdine);
Coda* init_coda();
void enqueue_pronti(Coda *coda, Ordine * nuovoOrdine, CatalogoRicette *cat);
Ingrediente *trova_ingrediente(CatalogoIngredienti *map, char nome[MAX_NOME]);

// divide la lista in due parti
void split_list(Nodo *source, Nodo **front, Nodo **back) {
    Nodo *fast;
    Nodo *slow;
    // Se la lista ha meno di due nodi, non possiamo dividerla
    if (source == NULL || source->prossimo == NULL) {
        *front = source;
        *back = NULL;
        return;
    }
    slow = source;
    fast = source->prossimo;
    // Fast si muove di due nodi, slow si muove di un nodo
    while (fast != NULL) {
        fast = fast->prossimo;
        if (fast != NULL) {
            slow = slow->prossimo;
            fast = fast->prossimo;
        }
    }
    // Slow è al nodo centrale, usato per dividere la lista
    *front = source;
    *back = slow->prossimo;
    slow->prossimo = NULL;
}

// ricombina le due liste ordinate
Nodo* sorted_merge(Nodo *a, Nodo *b) {
    Nodo *result = NULL;
    // Se uno dei due nodi è NULL, restituisci l'altro nodo
    if (a == NULL)
        return b;
    else if (b == NULL)
        return a;
    // Ricorsione per ordinare i nodi
    if (a->ordine->istante_arrivo <= b->ordine->istante_arrivo) {
        result = a;
        result->prossimo = sorted_merge(a->prossimo, b);
    } else {
        result = b;
        result->prossimo = sorted_merge(a, b->prossimo);
    }
    return result;
}

// Ordina la lista collegata in base all'istante di arrivo dell'ordine
void merge_sort(Nodo **head_ref) {
    Nodo *head = *head_ref;
    Nodo *a;
    Nodo *b;
    if (head == NULL || head->prossimo == NULL) {
        return;
    }
    split_list(head, &a, &b);
    merge_sort(&a);
    merge_sort(&b);
    *head_ref = sorted_merge(a, b);
}

// Ordina la coda in base all'istante di arrivo dell'ordine
void ordina_coda_per_istante_arrivo(Coda *coda) {
    //coda vuota
    if (coda == NULL || coda->fronte == NULL) {
        return;
    }
    merge_sort(&(coda->fronte));
    // Aggiornamento retro coda
    Nodo *current = coda->fronte;
    while (current->prossimo != NULL) {
        current = current->prossimo;
    }
    coda->retro = current;
    return;
}


Ingrediente *trova_ingrediente(CatalogoIngredienti *map, char nome[MAX_NOME]) {
    unsigned int index = hash(nome);
    Ingrediente *ing = map->buckets[index];
    while (ing) {
        if (strcmp(ing->nome, nome) == 0) {
            return ing;
        }
        ing = ing->next;
    }
    return NULL;
}

// inizializza magazzino
CatalogoIngredienti *init_map() {
    CatalogoIngredienti *map = (CatalogoIngredienti *)malloc(sizeof(CatalogoIngredienti));
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        map->buckets[i] = NULL;
    }
    return map;
}


void inserisci_lotto(FILE *file, CatalogoIngredienti *map) {
    char nome_ingrediente[MAX_NOME];
    int scadenza;
    int quantita;
    char terminatore = 'u';
    while(terminatore != '\n'){
        if(fscanf(file, "%s %d %d%c", nome_ingrediente, &quantita, &scadenza, &terminatore) != 4){
            return;
        }
        unsigned int index = hash(nome_ingrediente);
        Lotto *nuovoLotto = (Lotto *)malloc(sizeof(Lotto));
        nuovoLotto->scadenza = scadenza;
        nuovoLotto->quantita = quantita;
        nuovoLotto->next = NULL;
        // Insert the new lotto in the linked list in ascending order of scadenza
        if (map->buckets[index] == NULL) {
            map->buckets[index] = (Ingrediente *)malloc(sizeof(Ingrediente));
            strcpy(map->buckets[index]->nome, nome_ingrediente);
            map->buckets[index]->lotti = nuovoLotto;
            map->buckets[index]->next = NULL;
        }
        else{
            // GESTIONE COLLISIONI, AGGIUNTA IN TESTA
            Ingrediente *curr = map->buckets[index];
            while (curr != NULL && strcmp(curr->nome, nome_ingrediente) != 0) {
                    curr = curr->next;
                }
                //ingrediente non esiste
            if (curr == NULL) {
                Ingrediente *nuovoIngrediente = (Ingrediente *)malloc(sizeof(Ingrediente));
                strcpy(nuovoIngrediente->nome, nome_ingrediente);
                nuovoIngrediente->lotti = nuovoLotto;
                nuovoIngrediente->next = map->buckets[index];
                map->buckets[index] = nuovoIngrediente;
            } 
            else {
                Lotto *curr_lotto = curr->lotti;
                Lotto *prev = NULL;
                while (curr_lotto != NULL && curr_lotto->scadenza < scadenza) {
                    prev = curr_lotto;
                    curr_lotto = curr_lotto->next;
                }
                // controlla se esiste un lotto con scadenza uguale
                if (curr_lotto != NULL && curr_lotto->scadenza == scadenza) {
                    curr_lotto->quantita += quantita;
                    free(nuovoLotto);
                } else {
                    if (prev) {
                        prev->next = nuovoLotto;
                        nuovoLotto->next = curr_lotto;
                    } else {
                        nuovoLotto->next = curr->lotti;
                        curr->lotti = nuovoLotto;
                    }
                }
            }
        }
    }
    printf("rifornito\n");
}

// funzione di hashing, trovata su internet, spero che non mi bocciate se leggete questo commento
unsigned int hash(char str[MAX_NOME]) {
    unsigned int hash = 0;
    while (*str) {
        hash = (hash << 5) + *str++;  // hash * 32 + carattere corrente
    }
    return hash % HASH_MAP_SIZE;
}


void rimuovi_lotti(CatalogoIngredienti *map, CatalogoRicette *cat , Ordine *ordine, int istante_corrente) {
    Ricetta *ric = ordine->ricetta;
    Ingredienti_ricetta *curr = ric->ingredienti_necessari;
    // scorro ingredienti necessari per la ricetta
    while(curr){
        int quantita_necessaria = curr->quantita * ordine->quantita;
        Ingrediente *ing = curr->stock;
        Ingrediente *prev_ing = NULL;
        // scorro ingredienti in magazzino per trovare quello necessario tra le possibili collisioni
        //ingrediente in ing
        Lotto *curr_lotto = ing->lotti;
        Lotto *prev = NULL;
        while(curr_lotto != NULL && quantita_necessaria > 0){
            // rimuovo se il lotto è scaduto O se ha quantita nulla
            if(curr_lotto->scadenza <= istante_corrente || curr_lotto->quantita == 0){
                Lotto *temp = curr_lotto;
                if (prev) {
                    prev->next = curr_lotto->next;
                } else {
                    ing->lotti = curr_lotto->next;
                }
                curr_lotto = curr_lotto->next;
                free(temp);
                continue;
            }
            // rimuovo quantita necessaria rimanente
            if(curr_lotto->scadenza > istante_corrente && curr_lotto->quantita >= quantita_necessaria){
                curr_lotto->quantita -= quantita_necessaria;
                quantita_necessaria = 0;
                break;
            } else if(curr_lotto->scadenza > istante_corrente && curr_lotto->quantita < quantita_necessaria){
                quantita_necessaria -= curr_lotto->quantita;
                curr_lotto->quantita = 0;
                //libera lotto vuoto
                Lotto *temp = curr_lotto;
                if (prev) {
                    prev->next = curr_lotto->next;
                } else {
                    ing->lotti = curr_lotto->next;
                }
                curr_lotto = curr_lotto->next;
                free(temp);
                continue;
            }
            prev = curr_lotto;
            curr_lotto = curr_lotto->next;
        }
    //controllo se l'ingrediente non ha più lotti
    if(ing->lotti == NULL){
        if(prev_ing){
            prev_ing->next = ing->next;
        } else {
            unsigned int index = hash(ing->nome);
            map->buckets[index] = ing->next;
        }
        free(ing);
    }
    curr = curr->next;
    }
}

// libera la memoria allocata per la coda
void free_coda(Coda *coda) {
    while (coda->fronte) {
        Nodo* temp = coda->fronte;
        coda->fronte = coda->fronte->prossimo;
        free(temp);
    }
    free(coda);
    return;
}
// trova peso dell'ordine
int somma_quantita(CatalogoRicette *cat, Ordine *ordine) {
    Ricetta *ric = ordine->ricetta;
    int somma = 0;
    Ingredienti_ricetta *curr = ric->ingredienti_necessari;
    while (curr) {
        somma += curr->quantita * ordine->quantita;
        curr = curr->next;
    }
    return somma;
}

// enqueue ordinato per peso e istante di arrivo
void enqueue_ritiro(CatalogoRicette *cat, Coda *coda, Ordine *nuovoOrdine){
    Nodo* nuovoNodo = (Nodo*)malloc(sizeof(Nodo));
    nuovoNodo->ordine = nuovoOrdine;
    nuovoNodo->peso = somma_quantita(cat, nuovoOrdine);
    nuovoNodo->prossimo = NULL;
    if (coda->retro == NULL || coda->fronte == NULL) {
        // La coda è vuota, quindi sia fronte che retro devono puntare al nuovo nodo
        coda->fronte = nuovoNodo;
        coda->retro = nuovoNodo;
    } else {
        // Inserisci il nuovo nodo nella posizione corretta ordinando per peso dell'ordine
        int peso_nuovo = nuovoNodo->peso;
        Nodo* current = coda->fronte;
        Nodo* prev = NULL;
        while (current) {
            int peso_corrente = current->peso;
            if (peso_nuovo > peso_corrente || (peso_nuovo == peso_corrente && nuovoOrdine->istante_arrivo < current->ordine->istante_arrivo)) {
                // Inserisci il nuovo nodo prima del nodo corrente
                nuovoNodo->prossimo = current;
                if (prev) {
                    prev->prossimo = nuovoNodo;
                } else {
                    coda->fronte = nuovoNodo;
                }
                break;
            }
            prev = current;
            current = current->prossimo;
        }
        // Se il nuovo nodo non è stato inserito prima di nessun nodo, inseriscilo alla fine della coda
        if (current == NULL) {
            prev->prossimo = nuovoNodo;
            coda->retro = nuovoNodo;
        }
    }
}

// ritiro del camionciino
void ritiro(Coda *ordini_ritirati, Coda *ordini_pronti, int capacita, CatalogoRicette *cat) {
    int camioncino_vuoto = 1;
    int quantita_corrente = 0;
    Nodo* curr = ordini_pronti->fronte;
    Nodo* tmp; // free nodi
    while (curr) {
        quantita_corrente += curr->peso;
        if (quantita_corrente > capacita) {
            break;
        }
        camioncino_vuoto = 0;
        enqueue_ritiro(cat, ordini_ritirati, curr->ordine);
        // free nodo
        tmp = curr;
        ordini_pronti->fronte = curr->prossimo;
        curr = curr->prossimo;
        free(tmp);
    }
    //coda vuota
    if(ordini_pronti->fronte == NULL) {
        ordini_pronti->retro = NULL;
    }
    if (camioncino_vuoto) {
        printf("camioncino vuoto\n");
        return;
    }
    curr = ordini_ritirati->fronte;
    // stampa ordini_ritirati
    while (curr) {
        printf("%d %s %d\n",curr->ordine->istante_arrivo ,curr->ordine->nome_ricetta, curr->ordine->quantita);
        tmp = curr;
        curr = curr->prossimo;
        free(tmp);
    }
    //svuota ordini_ritirati
    ordini_ritirati->fronte = NULL;
    ordini_ritirati->retro = NULL;
}


void rimuovi_ricetta(FILE *file, CatalogoRicette *cat, Coda *ordini_in_attesa, Coda *ordini_pronti) {
    char nome_ricetta[MAX_NOME];
    if(fscanf(file, "%s", nome_ricetta) != 1){
        return;
    }
    Ricetta *ric = trova_ricetta(cat, nome_ricetta);
    if (ric) {
        // Controlla che la ricetta non sia in un ordine in attesa
        Nodo* curr = ordini_in_attesa->fronte;
        while (curr) {
            if (strcmp(curr->ordine->nome_ricetta, nome_ricetta) == 0) {
                printf("ordini in sospeso\n");
                return;
            }
            curr = curr->prossimo;
        }
        //Controllo se la ricetta è in un ordine pronto
        curr = ordini_pronti->fronte;
        while (curr) {
            if (strcmp(curr->ordine->nome_ricetta, nome_ricetta) == 0) {
                printf("ordini in sospeso\n");
                return;
            }
            curr = curr->prossimo;
        }
        // Rimuovi la ricetta
        free_catalogo(cat, nome_ricetta);
        printf("rimossa\n");
    } else {
        printf("non presente\n");
    }
}


void free_catalogo(CatalogoRicette *cat, char nome_ricetta[MAX_NOME]) {
    unsigned int index = hash(nome_ricetta);
    Ricetta *curr = cat->ricette[index];
    Ricetta *prev = NULL;
    while (curr) {
        if (strcmp(curr->nome, nome_ricetta) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                cat->ricette[index] = curr->next;
            }
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void controllo_rifornimento(CatalogoIngredienti *map, CatalogoRicette *cat, Coda *ordini_pronti, Coda *ordini_in_attesa, int istante_corrente) {
    Nodo* curr = ordini_in_attesa->fronte;
    Nodo* prev = NULL;
    while (curr){
        if (verifica_fattibilita(map, cat, curr->ordine, istante_corrente) == 1) {
            enqueue_pronti(ordini_pronti, curr->ordine, cat);
            rimuovi_lotti(map, cat, curr->ordine, istante_corrente);
            //libera nodo dalla coda in attesa
            Nodo* temp = curr;
            if (prev) {
                prev->prossimo = curr->prossimo;
            } else {
                ordini_in_attesa->fronte = curr->prossimo;
            }
            if(curr == ordini_in_attesa->retro){
                ordini_in_attesa->retro = prev;
            }
            curr = curr->prossimo;
            free(temp);
        } else {
            prev = curr;
            curr = curr->prossimo;
        }
    }

}


Coda* init_coda() {
    Coda* coda = (Coda*)malloc(sizeof(Coda));
    coda->fronte = NULL;
    coda->retro = NULL;
    return coda;
}


Ordine *init_ordine(FILE *file, int istante_arrivo, CatalogoRicette *cat) {
    Ordine *ordine = (Ordine *)malloc(sizeof(Ordine));
    if(fscanf(file, "%s %d", ordine->nome_ricetta, &ordine->quantita ) != 2){
        return ordine;
    }
    ordine->ricetta = trova_ricetta(cat, ordine->nome_ricetta);
    ordine->istante_arrivo = istante_arrivo;
    return ordine;
}


void gestione_ordine(CatalogoIngredienti *map, CatalogoRicette *cat, Coda *ordini_pronti, Coda *ordini_in_attesa, Ordine *ordine, int istante_corrente) {
    int codice_ordine = verifica_fattibilita(map, cat, ordine, istante_corrente);
    if (codice_ordine == 2){
        printf("rifiutato\n");
        return;
    }
    if (codice_ordine == 1){
        enqueue_pronti(ordini_pronti, ordine,cat);
        rimuovi_lotti(map, cat, ordine, istante_corrente);
        printf("accettato\n");
        return;
    }
    if(codice_ordine == 0){
        enqueue_pronti(ordini_in_attesa, ordine,cat);
        printf("accettato\n");
    }
    return;
}


int verifica_fattibilita(CatalogoIngredienti *map, CatalogoRicette *cat, Ordine *ordine, int istante_corrente) {
    //restituisci 0 se l'ordine è fattibile e va in attesa, 1 se l'ordine è fattibile e va in pronti, 2 se l'ordine non è fattibile
    Ricetta *ric = ordine->ricetta;
    if (!ric) {
        return 2;
    }
    Ingredienti_ricetta *curr = ric->ingredienti_necessari;
    while (curr) {
        Ingrediente *ing = curr->stock;
        if (ing->lotti == NULL) {
            return 0;
        }
        Lotto *curr_lotto = ing->lotti;
        Lotto *prev = NULL;
        int quantita_necessaria = curr->quantita * ordine->quantita;
        while (curr_lotto) {
            // rimuovo se il lotto è scaduto O se ha quantita nulla
            if(curr_lotto->scadenza <= istante_corrente || curr_lotto->quantita == 0){
                Lotto *temp = curr_lotto;
                if (prev) {
                    prev->next = curr_lotto->next;
                } else {
                    ing->lotti = curr_lotto->next;
                }
                curr_lotto = curr_lotto->next;
                if(temp){
                    free(temp);
                    temp = NULL;
                }
                continue;
            }
            // lotto contiene tutta la quantita necessaria
            if(curr_lotto->quantita >= quantita_necessaria){
                quantita_necessaria = 0;
                break;
            }
            // lotto contiene meno della quantita necessaria
            if(curr_lotto->quantita < quantita_necessaria){ 
                quantita_necessaria -= curr_lotto->quantita;
            }
            prev = curr_lotto;
            curr_lotto = curr_lotto->next;
        }
        if(quantita_necessaria > 0){
            return 0;
        }
        curr = curr->next;
    }
    return 1;
}


void enqueue_pronti(Coda *coda, Ordine *nuovoOrdine, CatalogoRicette *cat) {
    //inizializza nodo
    Nodo* nuovoNodo = (Nodo*)malloc(sizeof(Nodo));
    nuovoNodo->ordine = nuovoOrdine;
    nuovoNodo->peso = somma_quantita(cat, nuovoOrdine);
    nuovoNodo->prossimo = NULL;
    //coda vuota
    if (coda->fronte == NULL || coda->retro == NULL){
        coda->fronte = nuovoNodo;
        coda->retro = nuovoNodo;
        return;
    }
    //aggiungi in coda
    coda->retro->prossimo = nuovoNodo;
    coda->retro = nuovoNodo;
    return;
}

//FUNZIONI PER CATALOGO RICETTE
CatalogoRicette* init_catalogo() {
    CatalogoRicette* cat = (CatalogoRicette*)malloc(sizeof(CatalogoRicette));
    for(int i = 0; i < HASH_MAP_SIZE; i++){
        cat->ricette[i] = NULL;
    }
    return cat;
}


Ricetta* trova_ricetta(CatalogoRicette *cat, char nome[MAX_NOME]) {
    unsigned int index = hash(nome);
    Ricetta *ric = cat->ricette[index];
    while (ric) {
        if (strcmp(ric->nome, nome) == 0) {
            return ric;
        }
        ric = ric->next;
    }
    return NULL;
}


void aggiungi_ricetta(FILE *file, CatalogoRicette *cat, CatalogoIngredienti *map) {
    char nome_ricetta[MAX_NOME];
    if (fscanf(file, "%s", nome_ricetta) != 1){
        return;
    }
    unsigned int index = hash(nome_ricetta);
    //ignora ricetta
    if(trova_ricetta(cat, nome_ricetta)){
        printf("ignorato\n");
        // svuota buffer
        char c = '0';
        while((c = fgetc(file)) != '\n');
        return;
    }
    // inizializza ricetta
    Ricetta *nuovaRicetta = (Ricetta*)malloc(sizeof(Ricetta));
    strcpy(nuovaRicetta->nome, nome_ricetta);
    nuovaRicetta->ingredienti_necessari = NULL;
    // inizializza ingedienti
    int quantita;
    char terminatore = '0';
    char ingrediente[MAX_NOME];
    while(terminatore != '\n'){
        if(fscanf(file, "%s %d%c", ingrediente, &quantita, &terminatore) != 3){
            return;
        }
        Ingredienti_ricetta *nuovoIngrediente = (Ingredienti_ricetta*)malloc(sizeof(Ingredienti_ricetta));
        nuovoIngrediente->quantita = quantita;
        nuovoIngrediente->next = NULL;
        nuovoIngrediente->stock = trova_ingrediente(map, ingrediente);
        if(nuovoIngrediente->stock == NULL){
            //crea un nuovo ingrediente nel catalogo ingredienti e assegnalo al nuovoIngrediente
            unsigned int index = hash(ingrediente);
            Ingrediente *ing = map->buckets[index];
            while (ing && strcmp(ing->nome, ingrediente) != 0) {
                ing = ing->next;
            }
            if (ing == NULL) {
                Ingrediente *nuovoIngredienteMap = (Ingrediente *)malloc(sizeof(Ingrediente));
                strcpy(nuovoIngredienteMap->nome, ingrediente);
                nuovoIngredienteMap->lotti = NULL;
                nuovoIngredienteMap->next = map->buckets[index];
                map->buckets[index] = nuovoIngredienteMap;
                nuovoIngrediente->stock = nuovoIngredienteMap;
            } else {
                nuovoIngrediente->stock = ing;
            }
        }
        //aggiungi ingrediente in testa
        if (nuovaRicetta->ingredienti_necessari == NULL) {
            nuovaRicetta->ingredienti_necessari = nuovoIngrediente;
        } else {
            // aggiungi ingrediente in coda
            Ingredienti_ricetta *curr = nuovaRicetta->ingredienti_necessari;
            while (curr && curr->next) {
                curr = curr->next;
            }
            curr->next = nuovoIngrediente;
        }
    // add nuovaRicetta in catalogo
    }
    if (cat->ricette[index] == NULL) {
            cat->ricette[index] = nuovaRicetta;
            cat->ricette[index]->next = NULL;
        } else {
            Ricetta *curr = cat->ricette[index];
            while (curr && curr->next) {
                curr = curr->next;
            }
            curr->next = nuovaRicetta;
            curr->next->next = NULL;
        }
    printf("aggiunta\n");
}


int main() {
    FILE* file = stdin;
    if (file == NULL) {
        printf("Errore nell'apertura del file\n");
       return 1;
    }
    //inizializzazione oggetti
    CatalogoRicette* cat = init_catalogo();
    CatalogoIngredienti* map = init_map();
    Coda* ordini_pronti = init_coda();
    Coda* ordini_in_attesa = init_coda();
    Coda* ordini_ritirati = init_coda();
    // lettura dati
    int periodicita, capienza;    
    if(fscanf(file, "%u %u", &periodicita, &capienza) != 2){
        return 0;
    }
    int i = 0;
    char comando[MAX_NOME];
    while (fscanf(file,"%s", comando) == 1){
        if (i % periodicita == 0 && i != 0){
            ordina_coda_per_istante_arrivo(ordini_pronti);
            ritiro(ordini_ritirati, ordini_pronti, capienza, cat);
        }
        if (strcmp(comando, "aggiungi_ricetta") == 0) {
            aggiungi_ricetta(file, cat, map);
        } else if (strcmp(comando, "rimuovi_ricetta") == 0) {
            rimuovi_ricetta(file, cat, ordini_in_attesa, ordini_pronti);
        } else if (strcmp(comando, "rifornimento") == 0) {
            inserisci_lotto(file, map);
            controllo_rifornimento(map, cat, ordini_pronti, ordini_in_attesa, i);
        } else if (strcmp(comando, "ordine") == 0) {
            gestione_ordine(map, cat, ordini_pronti, ordini_in_attesa, init_ordine(file, i, cat), i);
        } else {
            printf("Comando non riconosciuto: %s\n", comando);
        }
        i++;
    }
    if (i % periodicita == 0 && i != 0){
        ritiro(ordini_ritirati, ordini_pronti, capienza, cat);
    }
    //free tutto
    free_coda(ordini_pronti);
    free_coda(ordini_in_attesa);
    free_coda(ordini_ritirati);
    fclose(file);
    return 0;
}