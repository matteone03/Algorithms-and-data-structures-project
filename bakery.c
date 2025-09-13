#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_NAME 20
#define HASH_MAP_SIZE 307

typedef struct Batch {
    int expiration; // Batch expiration time
    int quantity; // Product quantity in the batch
    struct Batch *next;  // For collision handling (linked list)
} Batch;

typedef struct Ingredient {
    Batch *batches;   // List of batches for an ingredient
    char name[MAX_NAME];    // Ingredient name
    struct Ingredient *next;  // For collision handling (linked list)
} Ingredient; // Linked list node

typedef struct IngredientCatalog {
    Ingredient *buckets[HASH_MAP_SIZE];  // Array of pointers to batches
} IngredientCatalog;

typedef struct RecipeIngredient {
    Ingredient *stock; // Pointer to ingredient in warehouse
    int quantity;    // Required quantity per ingredient
    struct RecipeIngredient *next;
} RecipeIngredient;

typedef struct Recipe {
    char name[MAX_NAME];    // Recipe name
    RecipeIngredient *required_ingredients; // List of ingredients needed for the recipe
    struct Recipe *next;
} Recipe;

typedef struct {
    Recipe *recipes[HASH_MAP_SIZE]; // hash table of recipes
} RecipeCatalog;

typedef struct {
    Recipe *recipe;   // Pointer to ordered recipe
    int arrival_time;   // Time when the order was received
    char recipe_name[MAX_NAME]; // Name of the ordered recipe
    int quantity;         // Number of desserts ordered
} Order;

// Linked list node representing a single order in the queue
typedef struct Node {
    int weight;   // Order weight (total quantity of ingredients needed)
    Order *order; // Pointer to order
    struct Node* next;
} Node;

// ready orders, waiting and picked up
typedef struct {
    Node* front;  // Pointer to node at front of queue (first element)
    Node* rear;   // Pointer to node at end of queue (last element)
} Queue;

// Function declarations
IngredientCatalog* init_ingredient_map();
RecipeCatalog* init_recipe_catalog();
unsigned int hash(char str[MAX_NAME]);
void insert_batch(FILE *file, IngredientCatalog *map);
void remove_batches(IngredientCatalog *map, RecipeCatalog *cat, Order *order, int current_time);
Recipe* find_recipe(RecipeCatalog *cat, char name[MAX_NAME]);
void add_recipe(FILE *file, RecipeCatalog *cat, IngredientCatalog *map);
void remove_recipe(FILE *file, RecipeCatalog *cat, Queue *waiting_orders, Queue *ready_orders);
void free_recipe_catalog(RecipeCatalog *cat, char recipe_name[MAX_NAME]);
int check_feasibility(IngredientCatalog *map, RecipeCatalog *cat, Order *order, int current_time);
void handle_order(IngredientCatalog *map, RecipeCatalog *cat, Queue *ready_orders, Queue *waiting_orders, Order *order, int current_time);
void check_restock(IngredientCatalog *map, RecipeCatalog *cat, Queue *ready_orders, Queue *waiting_orders, int current_time);
void pickup(Queue *picked_orders, Queue *ready_orders, int capacity, RecipeCatalog *cat);
int sum_quantities(RecipeCatalog *cat, Order *order);
Order *init_order(FILE *file, int arrival_time, RecipeCatalog *cat);
void enqueue_pickup(RecipeCatalog *cat, Queue* queue, Order *new_order);
Queue* init_queue();
void enqueue_ready(Queue *queue, Order *new_order, RecipeCatalog *cat);
Ingredient *find_ingredient(IngredientCatalog *map, char name[MAX_NAME]);

// Split the list into two parts
void split_list(Node *source, Node **front, Node **back) {
    Node *fast;
    Node *slow;
    // If the list has less than two nodes, we can't split it
    if (source == NULL || source->next == NULL) {
        *front = source;
        *back = NULL;
        return;
    }
    slow = source;
    fast = source->next;
    // Fast moves two nodes, slow moves one node
    while (fast != NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }
    // Slow is at the central node, used to split the list
    *front = source;
    *back = slow->next;
    slow->next = NULL;
}

// Recombine the two sorted lists
Node* sorted_merge(Node *a, Node *b) {
    Node *result = NULL;
    // If one of the two nodes is NULL, return the other node
    if (a == NULL)
        return b;
    else if (b == NULL)
        return a;
    // Recursion to sort the nodes
    if (a->order->arrival_time <= b->order->arrival_time) {
        result = a;
        result->next = sorted_merge(a->next, b);
    } else {
        result = b;
        result->next = sorted_merge(a, b->next);
    }
    return result;
}

// Sort the linked list based on order arrival time
void merge_sort(Node **head_ref) {
    Node *head = *head_ref;
    Node *a;
    Node *b;
    if (head == NULL || head->next == NULL) {
        return;
    }
    split_list(head, &a, &b);
    merge_sort(&a);
    merge_sort(&b);
    *head_ref = sorted_merge(a, b);
}

// Sort the queue based on order arrival time
void sort_queue_by_arrival_time(Queue *queue) {
    // empty queue
    if (queue == NULL || queue->front == NULL) {
        return;
    }
    merge_sort(&(queue->front));
    // Update queue rear
    Node *current = queue->front;
    while (current->next != NULL) {
        current = current->next;
    }
    queue->rear = current;
    return;
}

Ingredient *find_ingredient(IngredientCatalog *map, char name[MAX_NAME]) {
    unsigned int index = hash(name);
    Ingredient *ing = map->buckets[index];
    while (ing) {
        if (strcmp(ing->name, name) == 0) {
            return ing;
        }
        ing = ing->next;
    }
    return NULL;
}

// Initialize warehouse
IngredientCatalog *init_ingredient_map() {
    IngredientCatalog *map = (IngredientCatalog *)malloc(sizeof(IngredientCatalog));
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        map->buckets[i] = NULL;
    }
    return map;
}

void insert_batch(FILE *file, IngredientCatalog *map) {
    char ingredient_name[MAX_NAME];
    int expiration;
    int quantity;
    char terminator = 'u';
    while(terminator != '\n'){
        if(fscanf(file, "%s %d %d%c", ingredient_name, &quantity, &expiration, &terminator) != 4){
            return;
        }
        unsigned int index = hash(ingredient_name);
        Batch *new_batch = (Batch *)malloc(sizeof(Batch));
        new_batch->expiration = expiration;
        new_batch->quantity = quantity;
        new_batch->next = NULL;
        // Insert the new batch in the linked list in ascending order of expiration
        if (map->buckets[index] == NULL) {
            map->buckets[index] = (Ingredient *)malloc(sizeof(Ingredient));
            strcpy(map->buckets[index]->name, ingredient_name);
            map->buckets[index]->batches = new_batch;
            map->buckets[index]->next = NULL;
        }
        else{
            // COLLISION HANDLING, ADD TO HEAD
            Ingredient *curr = map->buckets[index];
            while (curr != NULL && strcmp(curr->name, ingredient_name) != 0) {
                    curr = curr->next;
                }
                // ingredient doesn't exist
            if (curr == NULL) {
                Ingredient *new_ingredient = (Ingredient *)malloc(sizeof(Ingredient));
                strcpy(new_ingredient->name, ingredient_name);
                new_ingredient->batches = new_batch;
                new_ingredient->next = map->buckets[index];
                map->buckets[index] = new_ingredient;
            } 
            else {
                Batch *curr_batch = curr->batches;
                Batch *prev = NULL;
                while (curr_batch != NULL && curr_batch->expiration < expiration) {
                    prev = curr_batch;
                    curr_batch = curr_batch->next;
                }
                // check if a batch with the same expiration exists
                if (curr_batch != NULL && curr_batch->expiration == expiration) {
                    curr_batch->quantity += quantity;
                    free(new_batch);
                } else {
                    if (prev) {
                        prev->next = new_batch;
                        new_batch->next = curr_batch;
                    } else {
                        new_batch->next = curr->batches;
                        curr->batches = new_batch;
                    }
                }
            }
        }
    }
    printf("restocked\n");
}

// Hash function, found on internet, hope you don't fail me if you read this comment
unsigned int hash(char str[MAX_NAME]) {
    unsigned int hash = 0;
    while (*str) {
        hash = (hash << 5) + *str++;  // hash * 32 + current character
    }
    return hash % HASH_MAP_SIZE;
}

void remove_batches(IngredientCatalog *map, RecipeCatalog *cat, Order *order, int current_time) {
    Recipe *recipe = order->recipe;
    RecipeIngredient *curr = recipe->required_ingredients;
    // iterate through ingredients needed for the recipe
    while(curr){
        int required_quantity = curr->quantity * order->quantity;
        Ingredient *ing = curr->stock;
        Ingredient *prev_ing = NULL;
        // iterate through ingredients in warehouse to find the needed one among possible collisions
        // ingredient in ing
        Batch *curr_batch = ing->batches;
        Batch *prev = NULL;
        while(curr_batch != NULL && required_quantity > 0){
            // remove if batch is expired OR has zero quantity
            if(curr_batch->expiration <= current_time || curr_batch->quantity == 0){
                Batch *temp = curr_batch;
                if (prev) {
                    prev->next = curr_batch->next;
                } else {
                    ing->batches = curr_batch->next;
                }
                curr_batch = curr_batch->next;
                free(temp);
                continue;
            }
            // remove remaining required quantity
            if(curr_batch->expiration > current_time && curr_batch->quantity >= required_quantity){
                curr_batch->quantity -= required_quantity;
                required_quantity = 0;
                break;
            } else if(curr_batch->expiration > current_time && curr_batch->quantity < required_quantity){
                required_quantity -= curr_batch->quantity;
                curr_batch->quantity = 0;
                // free empty batch
                Batch *temp = curr_batch;
                if (prev) {
                    prev->next = curr_batch->next;
                } else {
                    ing->batches = curr_batch->next;
                }
                curr_batch = curr_batch->next;
                free(temp);
                continue;
            }
            prev = curr_batch;
            curr_batch = curr_batch->next;
        }
    // check if ingredient has no more batches
    if(ing->batches == NULL){
        if(prev_ing){
            prev_ing->next = ing->next;
        } else {
            unsigned int index = hash(ing->name);
            map->buckets[index] = ing->next;
        }
        free(ing);
    }
    curr = curr->next;
    }
}

// Free allocated memory for the queue
void free_queue(Queue *queue) {
    while (queue->front) {
        Node* temp = queue->front;
        queue->front = queue->front->next;
        free(temp);
    }
    free(queue);
    return;
}

// Find order weight
int sum_quantities(RecipeCatalog *cat, Order *order) {
    Recipe *recipe = order->recipe;
    int sum = 0;
    RecipeIngredient *curr = recipe->required_ingredients;
    while (curr) {
        sum += curr->quantity * order->quantity;
        curr = curr->next;
    }
    return sum;
}

// Enqueue ordered by weight and arrival time
void enqueue_pickup(RecipeCatalog *cat, Queue *queue, Order *new_order){
    Node* new_node = (Node*)malloc(sizeof(Node));
    new_node->order = new_order;
    new_node->weight = sum_quantities(cat, new_order);
    new_node->next = NULL;
    if (queue->rear == NULL || queue->front == NULL) {
        // Queue is empty, so both front and rear must point to the new node
        queue->front = new_node;
        queue->rear = new_node;
    } else {
        // Insert the new node in the correct position ordering by order weight
        int new_weight = new_node->weight;
        Node* current = queue->front;
        Node* prev = NULL;
        while (current) {
            int current_weight = current->weight;
            if (new_weight > current_weight || (new_weight == current_weight && new_order->arrival_time < current->order->arrival_time)) {
                // Insert the new node before the current node
                new_node->next = current;
                if (prev) {
                    prev->next = new_node;
                } else {
                    queue->front = new_node;
                }
                break;
            }
            prev = current;
            current = current->next;
        }
        // If the new node hasn't been inserted before any node, insert it at the end of the queue
        if (current == NULL) {
            prev->next = new_node;
            queue->rear = new_node;
        }
    }
}

// Pickup by truck
void pickup(Queue *picked_orders, Queue *ready_orders, int capacity, RecipeCatalog *cat) {
    int truck_empty = 1;
    int current_quantity = 0;
    Node* curr = ready_orders->front;
    Node* tmp; // free nodes
    while (curr) {
        current_quantity += curr->weight;
        if (current_quantity > capacity) {
            break;
        }
        truck_empty = 0;
        enqueue_pickup(cat, picked_orders, curr->order);
        // free node
        tmp = curr;
        ready_orders->front = curr->next;
        curr = curr->next;
        free(tmp);
    }
    // empty queue
    if(ready_orders->front == NULL) {
        ready_orders->rear = NULL;
    }
    if (truck_empty) {
        printf("truck empty\n");
        return;
    }
    curr = picked_orders->front;
    // print picked_orders
    while (curr) {
        printf("%d %s %d\n",curr->order->arrival_time ,curr->order->recipe_name, curr->order->quantity);
        tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    // empty picked_orders
    picked_orders->front = NULL;
    picked_orders->rear = NULL;
}

void remove_recipe(FILE *file, RecipeCatalog *cat, Queue *waiting_orders, Queue *ready_orders) {
    char recipe_name[MAX_NAME];
    if(fscanf(file, "%s", recipe_name) != 1){
        return;
    }
    Recipe *recipe = find_recipe(cat, recipe_name);
    if (recipe) {
        // Check that the recipe is not in a waiting order
        Node* curr = waiting_orders->front;
        while (curr) {
            if (strcmp(curr->order->recipe_name, recipe_name) == 0) {
                printf("orders pending\n");
                return;
            }
            curr = curr->next;
        }
        // Check if the recipe is in a ready order
        curr = ready_orders->front;
        while (curr) {
            if (strcmp(curr->order->recipe_name, recipe_name) == 0) {
                printf("orders pending\n");
                return;
            }
            curr = curr->next;
        }
        // Remove the recipe
        free_recipe_catalog(cat, recipe_name);
        printf("removed\n");
    } else {
        printf("not present\n");
    }
}

void free_recipe_catalog(RecipeCatalog *cat, char recipe_name[MAX_NAME]) {
    unsigned int index = hash(recipe_name);
    Recipe *curr = cat->recipes[index];
    Recipe *prev = NULL;
    while (curr) {
        if (strcmp(curr->name, recipe_name) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                cat->recipes[index] = curr->next;
            }
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void check_restock(IngredientCatalog *map, RecipeCatalog *cat, Queue *ready_orders, Queue *waiting_orders, int current_time) {
    Node* curr = waiting_orders->front;
    Node* prev = NULL;
    while (curr){
        if (check_feasibility(map, cat, curr->order, current_time) == 1) {
            enqueue_ready(ready_orders, curr->order, cat);
            remove_batches(map, cat, curr->order, current_time);
            // free node from waiting queue
            Node* temp = curr;
            if (prev) {
                prev->next = curr->next;
            } else {
                waiting_orders->front = curr->next;
            }
            if(curr == waiting_orders->rear){
                waiting_orders->rear = prev;
            }
            curr = curr->next;
            free(temp);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

Queue* init_queue() {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->front = NULL;
    queue->rear = NULL;
    return queue;
}

Order *init_order(FILE *file, int arrival_time, RecipeCatalog *cat) {
    Order *order = (Order *)malloc(sizeof(Order));
    if(fscanf(file, "%s %d", order->recipe_name, &order->quantity ) != 2){
        return order;
    }
    order->recipe = find_recipe(cat, order->recipe_name);
    order->arrival_time = arrival_time;
    return order;
}

void handle_order(IngredientCatalog *map, RecipeCatalog *cat, Queue *ready_orders, Queue *waiting_orders, Order *order, int current_time) {
    int order_code = check_feasibility(map, cat, order, current_time);
    if (order_code == 2){
        printf("rejected\n");
        return;
    }
    if (order_code == 1){
        enqueue_ready(ready_orders, order, cat);
        remove_batches(map, cat, order, current_time);
        printf("accepted\n");
        return;
    }
    if(order_code == 0){
        enqueue_ready(waiting_orders, order, cat);
        printf("accepted\n");
    }
    return;
}

int check_feasibility(IngredientCatalog *map, RecipeCatalog *cat, Order *order, int current_time) {
    // return 0 if order is feasible and goes to waiting, 1 if order is feasible and goes to ready, 2 if order is not feasible
    Recipe *recipe = order->recipe;
    if (!recipe) {
        return 2;
    }
    RecipeIngredient *curr = recipe->required_ingredients;
    while (curr) {
        Ingredient *ing = curr->stock;
        if (ing->batches == NULL) {
            return 0;
        }
        Batch *curr_batch = ing->batches;
        Batch *prev = NULL;
        int required_quantity = curr->quantity * order->quantity;
        while (curr_batch) {
            // remove if batch is expired OR has zero quantity
            if(curr_batch->expiration <= current_time || curr_batch->quantity == 0){
                Batch *temp = curr_batch;
                if (prev) {
                    prev->next = curr_batch->next;
                } else {
                    ing->batches = curr_batch->next;
                }
                curr_batch = curr_batch->next;
                if(temp){
                    free(temp);
                    temp = NULL;
                }
                continue;
            }
            // batch contains all required quantity
            if(curr_batch->quantity >= required_quantity){
                required_quantity = 0;
                break;
            }
            // batch contains less than required quantity
            if(curr_batch->quantity < required_quantity){ 
                required_quantity -= curr_batch->quantity;
            }
            prev = curr_batch;
            curr_batch = curr_batch->next;
        }
        if(required_quantity > 0){
            return 0;
        }
        curr = curr->next;
    }
    return 1;
}

void enqueue_ready(Queue *queue, Order *new_order, RecipeCatalog *cat) {
    // initialize node
    Node* new_node = (Node*)malloc(sizeof(Node));
    new_node->order = new_order;
    new_node->weight = sum_quantities(cat, new_order);
    new_node->next = NULL;
    // empty queue
    if (queue->front == NULL || queue->rear == NULL){
        queue->front = new_node;
        queue->rear = new_node;
        return;
    }
    // add to queue
    queue->rear->next = new_node;
    queue->rear = new_node;
    return;
}

// FUNCTIONS FOR RECIPE CATALOG
RecipeCatalog* init_recipe_catalog() {
    RecipeCatalog* cat = (RecipeCatalog*)malloc(sizeof(RecipeCatalog));
    for(int i = 0; i < HASH_MAP_SIZE; i++){
        cat->recipes[i] = NULL;
    }
    return cat;
}

Recipe* find_recipe(RecipeCatalog *cat, char name[MAX_NAME]) {
    unsigned int index = hash(name);
    Recipe *recipe = cat->recipes[index];
    while (recipe) {
        if (strcmp(recipe->name, name) == 0) {
            return recipe;
        }
        recipe = recipe->next;
    }
    return NULL;
}

void add_recipe(FILE *file, RecipeCatalog *cat, IngredientCatalog *map) {
    char recipe_name[MAX_NAME];
    if (fscanf(file, "%s", recipe_name) != 1){
        return;
    }
    unsigned int index = hash(recipe_name);
    // ignore recipe
    if(find_recipe(cat, recipe_name)){
        printf("ignored\n");
        // clear buffer
        char c = '0';
        while((c = fgetc(file)) != '\n');
        return;
    }
    // initialize recipe
    Recipe *new_recipe = (Recipe*)malloc(sizeof(Recipe));
    strcpy(new_recipe->name, recipe_name);
    new_recipe->required_ingredients = NULL;
    // initialize ingredients
    int quantity;
    char terminator = '0';
    char ingredient[MAX_NAME];
    while(terminator != '\n'){
        if(fscanf(file, "%s %d%c", ingredient, &quantity, &terminator) != 3){
            return;
        }
        RecipeIngredient *new_ingredient = (RecipeIngredient*)malloc(sizeof(RecipeIngredient));
        new_ingredient->quantity = quantity;
        new_ingredient->next = NULL;
        new_ingredient->stock = find_ingredient(map, ingredient);
        if(new_ingredient->stock == NULL){
            // create a new ingredient in the ingredient catalog and assign it to new_ingredient
            unsigned int index = hash(ingredient);
            Ingredient *ing = map->buckets[index];
            while (ing && strcmp(ing->name, ingredient) != 0) {
                ing = ing->next;
            }
            if (ing == NULL) {
                Ingredient *new_ingredient_map = (Ingredient *)malloc(sizeof(Ingredient));
                strcpy(new_ingredient_map->name, ingredient);
                new_ingredient_map->batches = NULL;
                new_ingredient_map->next = map->buckets[index];
                map->buckets[index] = new_ingredient_map;
                new_ingredient->stock = new_ingredient_map;
            } else {
                new_ingredient->stock = ing;
            }
        }
        // add ingredient to head
        if (new_recipe->required_ingredients == NULL) {
            new_recipe->required_ingredients = new_ingredient;
        } else {
            // add ingredient to tail
            RecipeIngredient *curr = new_recipe->required_ingredients;
            while (curr && curr->next) {
                curr = curr->next;
            }
            curr->next = new_ingredient;
        }
    // add new_recipe to catalog
    }
    if (cat->recipes[index] == NULL) {
            cat->recipes[index] = new_recipe;
            cat->recipes[index]->next = NULL;
        } else {
            Recipe *curr = cat->recipes[index];
            while (curr && curr->next) {
                curr = curr->next;
            }
            curr->next = new_recipe;
            curr->next->next = NULL;
        }
    printf("added\n");
}

int main() {
    FILE* file = stdin;
    if (file == NULL) {
        printf("Error opening file\n");
       return 1;
    }
    // object initialization
    RecipeCatalog* cat = init_recipe_catalog();
    IngredientCatalog* map = init_ingredient_map();
    Queue* ready_orders = init_queue();
    Queue* waiting_orders = init_queue();
    Queue* picked_orders = init_queue();
    // data reading
    int periodicity, capacity;    
    if(fscanf(file, "%u %u", &periodicity, &capacity) != 2){
        return 0;
    }
    int i = 0;
    char command[MAX_NAME];
    while (fscanf(file,"%s", command) == 1){
        if (i % periodicity == 0 && i != 0){
            sort_queue_by_arrival_time(ready_orders);
            pickup(picked_orders, ready_orders, capacity, cat);
        }
        if (strcmp(command, "add_recipe") == 0) {
            add_recipe(file, cat, map);
        } else if (strcmp(command, "remove_recipe") == 0) {
            remove_recipe(file, cat, waiting_orders, ready_orders);
        } else if (strcmp(command, "restock") == 0) {
            insert_batch(file, map);
            check_restock(map, cat, ready_orders, waiting_orders, i);
        } else if (strcmp(command, "order") == 0) {
            handle_order(map, cat, ready_orders, waiting_orders, init_order(file, i, cat), i);
        } else {
            printf("Unrecognized command: %s\n", command);
        }
        i++;
    }
    if (i % periodicity == 0 && i != 0){
        pickup(picked_orders, ready_orders, capacity, cat);
    }
    // free everything
    free_queue(ready_orders);
    free_queue(waiting_orders);
    free_queue(picked_orders);
    fclose(file);
    return 0;
}
