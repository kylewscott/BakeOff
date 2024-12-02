#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

// Color codes for different baker outputs
#define RESET "\x1b[0m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"

// Maximum number of bakers
#define MAX_BAKERS 10

// Ingredients
typedef enum 
{
	FLOUR, SUGAR, YEAST, BAKING_SODA, SALT, CINNAMON,   // Pantry ingredients
    	EGG, MILK, BUTTER,                                  // Refrigerator ingredients
    	INGREDIENT_COUNT
} Ingredient;

// Recipes
typedef enum 
{
	COOKIES,
    	PANCAKES,
    	PIZZA_DOUGH,
    	SOFT_PRETZELS,
    	CINNAMON_ROLLS,
    	RECIPE_COUNT
} Recipe;

// Ingredient requirements for each recipe
Ingredient recipe_ingredients[][7] = 
{
	[COOKIES] = {FLOUR, SUGAR, MILK, BUTTER, -1},
    	[PANCAKES] = {FLOUR, SUGAR, BAKING_SODA, SALT, EGG, MILK, BUTTER},
    	[PIZZA_DOUGH] = {YEAST, SUGAR, SALT, -1},
    	[SOFT_PRETZELS] = {FLOUR, SUGAR, SALT, YEAST, BAKING_SODA, EGG, -1},
    	[CINNAMON_ROLLS] = {FLOUR, SUGAR, SALT, BUTTER, EGG, CINNAMON, -1}
};

// Shared kitchen resources
typedef struct 
{
	sem_t pantry;              // Binary semaphore for pantry access
    	sem_t refrigerators[2];    // Binary semaphores for refrigerators
    	sem_t mixers;              // Counting semaphore for mixers
    	sem_t bowls;               // Counting semaphore for bowls
    	sem_t spoons;              // Counting semaphore for spoons
    	sem_t ovens;               // Binary semaphore for ovens

    	// Ingredient availability tracking
    	int ingredient_available[INGREDIENT_COUNT];
    	sem_t ingredient_locks[INGREDIENT_COUNT];

    	int total_bakers;
    	int ramsay_target;         // Baker who will be 'Ramsied'
} KitchenResources;

// Baker thread data
typedef struct 
{
	int id;
    	char* color;
    	KitchenResources* kitchen;
} Baker;

// Global kitchen resources
KitchenResources kitchen;

// Function prototypes
void initialize_kitchen(int num_bakers);
void* baker_thread(void* arg);
int try_acquire_ingredients(Baker* baker, Recipe recipe);
void release_ingredients(Baker* baker, Recipe recipe);
void mix_ingredients(Baker* baker);
void bake_recipe(Baker* baker);
void ramsay_interrupt(Baker* baker);

// Initialize kitchen resources
void initialize_kitchen(int num_bakers) 
{
	// Initialize semaphores
    	sem_init(&kitchen.pantry, 0, 1);
    	sem_init(&kitchen.refrigerators[0], 0, 1);
    	sem_init(&kitchen.refrigerators[1], 0, 1);
    	sem_init(&kitchen.mixers, 0, 2);
    	sem_init(&kitchen.bowls, 0, 3);
    	sem_init(&kitchen.spoons, 0, 5);
    	sem_init(&kitchen.ovens, 0, 1);

    	// Initialize ingredient locks
    	for (int i = 0; i < INGREDIENT_COUNT; i++) 
	{
        	sem_init(&kitchen.ingredient_locks[i], 0, 1);
        	kitchen.ingredient_available[i] = 1;  // All ingredients are initially available
    	}

    	// Set total bakers and Ramsay target
    	kitchen.total_bakers = num_bakers;
    	srand(time(NULL));
    	kitchen.ramsay_target = rand() % num_bakers;
}

// Baker thread main function
void* baker_thread(void* arg) 
{
	Baker* baker = (Baker*)arg;
    
    	// Attempt each recipe
    	for (Recipe recipe = 0; recipe < RECIPE_COUNT; recipe++) 
	{
        	int ramsay_trigger = (baker->id == kitchen.ramsay_target) && (rand() % 2 == 0);
        
        	if (ramsay_trigger) 
		{
			ramsay_interrupt(baker);
            		continue;
        	}

        	// Try to acquire ingredients
        	printf("%sBaker %d is attempting to make recipe %d%s\n", baker->color, baker->id, recipe, RESET);
        
        	if (try_acquire_ingredients(baker, recipe)) 
		{
			mix_ingredients(baker);
            		bake_recipe(baker);
            		release_ingredients(baker, recipe);
            
            		printf("%sBaker %d completed recipe %d%s\n", 
                   	baker->color, baker->id, recipe, RESET);
        	}
    	}
    	return NULL;
}

// Simulate Gordon Ramsay 
void ramsay_interrupt(Baker* baker) 
{
	printf("%s GORDON RAMSAY INTERRUPTION FOR BAKER %d! %s\n", baker->color, baker->id, RESET);
    
	// Release all currently held semaphores
    	for (int i = 0; i < INGREDIENT_COUNT; i++) 
	{
        	sem_post(&kitchen.ingredient_locks[i]);
    	}
    
	sem_post(&kitchen.pantry);
    	sem_post(&kitchen.refrigerators[0]);
    	sem_post(&kitchen.refrigerators[1]);
    	sem_post(&kitchen.mixers);
    	sem_post(&kitchen.bowls);
    	sem_post(&kitchen.spoons);
    	sem_post(&kitchen.ovens);
}

// Main function with thread creation
int main() 
{
	int num_bakers;
    	printf("Enter number of bakers (max %d): ", MAX_BAKERS);
    	scanf("%d", &num_bakers);

    	if (num_bakers < 1 || num_bakers > MAX_BAKERS) 
	{
        	printf("Invalid number of bakers. Must be between 1 and %d\n", MAX_BAKERS);
        	return 1;
    	}

    	// Baker color array
    	char* baker_colors[] = {RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN};

    	// Initialize kitchen
    	initialize_kitchen(num_bakers);

    	// Create baker threads
    	pthread_t baker_threads[MAX_BAKERS];
    	Baker bakers[MAX_BAKERS];

    	for (int i = 0; i < num_bakers; i++) 
	{
        	bakers[i].id = i;
        	bakers[i].color = baker_colors[i % 6];
        	bakers[i].kitchen = &kitchen;
        
        	pthread_create(&baker_threads[i], NULL, baker_thread, &bakers[i]);
    	}

    	// Wait for all baker threads to complete
    	for (int i = 0; i < num_bakers; i++) 
	{
        	pthread_join(baker_threads[i], NULL);
    	}

    	printf("\n All bakers have finished their recipes! \n");
    	return 0;
}

// STILL NEED TO DO REMAINING FUNCTIONS
int try_acquire_ingredients(Baker* baker, Recipe recipe) 
{
	// Implementation for aquiring ingredients
}

void release_ingredients(Baker* baker, Recipe recipe) 
{
	// Implementation for releasing ingredients
}

void mix_ingredients(Baker* baker) 
{
	// Acquire mixer, bowl, and spoon
    	sem_wait(&kitchen.mixers);
    	sem_wait(&kitchen.bowls);
    	sem_wait(&kitchen.spoons);

    	printf("%sBaker %d is mixing ingredients%s\n", baker->color, baker->id, RESET);
    
    	sleep(1);  // Simulate mixing time

    	// Release mixer, bowl, and spoon
   	sem_post(&kitchen.mixers);
    	sem_post(&kitchen.bowls);
    	sem_post(&kitchen.spoons);
}

void bake_recipe(Baker* baker) 
{
	// Acquire oven
    	sem_wait(&kitchen.ovens);

    	printf("%sBaker %d is baking the recipe%s\n", baker->color, baker->id, RESET);
    
    	sleep(2);  // Simulate baking time

    	// Release oven
    	sem_post(&kitchen.ovens);
}
