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

Ingredient pantry_ingredients[] = 
{
    FLOUR, SUGAR, YEAST, BAKING_SODA, SALT, CINNAMON, -1
};

Ingredient fridge_ingredients[] =
{
    EGG, MILK, BUTTER, -1
};

char* recipe_names[] =
{
    "Cookies",
    "Pancakes",
    "Pizza Dough",
    "Soft Pretzels",
    "Cinnamon Rolls"
};

char* ingredient_names[] =
{
    "Flour","Sugar", "Yeast", "Baking Soda", "Salt", "Cinnamon",
    "Egg", "Milk", "Butter"
};

// Ingredient requirements for each recipe
Ingredient recipe_ingredients[][8] = 
{
    [COOKIES] = {FLOUR, SUGAR, MILK, BUTTER, -1},
    [PANCAKES] = {FLOUR, SUGAR, BAKING_SODA, SALT, EGG, MILK, BUTTER, -1},
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

// Track if ramsied has occured yet
int ramsay_triggered = 0;

// Global kitchen resources
KitchenResources kitchen;

// Function prototypes
void initialize_kitchen(int num_bakers);
void* baker_thread(void* arg);
int acquire_ingredients(Baker* baker, Recipe recipe);
void release_ingredients(Baker* baker, Recipe recipe);
int mix_ingredients(Baker* baker, Recipe recipe);
int bake_recipe(Baker* baker, Recipe recipe);
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
        while(1)
        {
            printf("%sBaker %d is attempting to make %s%s\n", baker->color, baker->id, recipe_names[recipe], RESET);
            if (acquire_ingredients(baker, recipe)) 
		    {
                if(mix_ingredients(baker, recipe))
                {
                    release_ingredients(baker, recipe);
                    if(bake_recipe(baker, recipe))
                    {
                        break;
                    }
                }
            }
        }
    }
    return NULL;
}

// Simulate Gordon Ramsay 
void ramsay_interrupt(Baker* baker) 
{
	printf("%s**GORDON RAMSAY INTERRUPTION FOR BAKER %d!**%s\n", baker->color, baker->id, RESET);
    printf("%sBaker %d has dropped everything and is restarting%s\n", baker->color, baker->id, RESET);

	// Release all currently held semaphores
    for (int i = 0; i < INGREDIENT_COUNT; i++)
    {
        if (kitchen.ingredient_available[i] == 0)
        {
            kitchen.ingredient_available[i]++;
       	    sem_post(&kitchen.ingredient_locks[i]);
        }
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

int check_pantry(Baker* baker, Recipe recipe)
{
    srand(time(NULL));

    printf("%sBaker %d is entering the pantry %s\n", baker->color, baker->id, RESET);

    for(int i = 0; recipe_ingredients[recipe][i] != -1; i++)
    {
        int ramsay_trigger = (!ramsay_triggered) && (baker->id == kitchen.ramsay_target) && (rand() % 5 == 0);
        if (ramsay_trigger)
        {
            ramsay_interrupt(baker);
            ramsay_triggered = 1;
            return 0;
        }
 
        Ingredient ingredient = recipe_ingredients[recipe][i];

        for(int j = 0; pantry_ingredients[j] != -1; j++)
        {
            if(ingredient == pantry_ingredients[j])
            {
                sem_wait(&kitchen.ingredient_locks[ingredient]);
 
                while(kitchen.ingredient_available[ingredient] == 0)
                {
                    sem_post(&kitchen.ingredient_locks[ingredient]);
                    sem_wait(&kitchen.ingredient_locks[ingredient]);
                }
 
                printf("%sBaker %d has gathered %s%s\n", baker->color, baker->id, ingredient_names[ingredient], RESET);
 
                kitchen.ingredient_available[ingredient]--;
                sem_post(&kitchen.ingredient_locks[ingredient]);
            }   
        }
    }
    
    return 1;
}

int check_fridge(Baker* baker, Recipe recipe)
{
    srand(time(NULL));

    for(int i = 0; recipe_ingredients[recipe][i] != -1; i++)
    {  
        int ramsay_trigger = (!ramsay_triggered) && (baker->id == kitchen.ramsay_target) && (rand() % 5 == 0); 
        if (ramsay_trigger)
        {
            ramsay_interrupt(baker);
            ramsay_triggered = 1;
            return 0;
        }        

        Ingredient ingredient = recipe_ingredients[recipe][i];

        for(int n = 0; fridge_ingredients[n] != -1; n++)    
        {
            if(ingredient == fridge_ingredients[n])
            {
                sem_wait(&kitchen.ingredient_locks[ingredient]);
 
                while(kitchen.ingredient_available[ingredient] == 0)
                {
                    sem_post(&kitchen.ingredient_locks[ingredient]);
                    sem_wait(&kitchen.ingredient_locks[ingredient]);
                }
 
                printf("%sBaker %d has gathered %s%s\n", baker->color, baker->id, ingredient_names[ingredient], RESET);
 
                kitchen.ingredient_available[ingredient]--;
                sem_post(&kitchen.ingredient_locks[ingredient]);
            }
        }
    }

    return 1;
}

int acquire_ingredients(Baker* baker, Recipe recipe) 
{
    printf("%sBaker %d is trying to aquire ingredients for %s%s\n", baker->color, baker->id, recipe_names[recipe], RESET);
    
    if(sem_trywait(&kitchen.pantry) == 0)
    {
        if(!check_pantry(baker, recipe))
        {
            return 0;
        }

        printf("%sBaker %d is leaving the pantry%s\n", baker->color, baker->id, RESET);

        sem_post(&kitchen.pantry);        

        if(sem_trywait(&kitchen.refrigerators[0]) == 0)
        {
            printf("%sBaker %d is entering fridge 0%s\n", baker->color, baker->id, RESET);

            if(!check_fridge(baker, recipe))
            {
                return 0;   
            }

            printf("%sBaker %d is leaving fridge 0%s\n", baker->color, baker->id, RESET);

            sem_post(&kitchen.refrigerators[0]);
        }

        else{
            printf("%sBaker %d is entering fridge 1%s\n", baker->color, baker->id, RESET);
    
            sem_wait(&kitchen.refrigerators[1]);

            if(!check_fridge(baker, recipe))
            {
                return 0;
            }

            printf("%sBaker %d is leaving fridge 1%s\n", baker->color, baker->id, RESET);

            sem_post(&kitchen.refrigerators[1]);
        } 
    }

    else{
        if (sem_trywait(&kitchen.refrigerators[0]) == 0) 
        {
            printf("%sBaker %d is entering fridge 0%s\n", baker->color, baker->id, RESET);
    
            if(!check_fridge(baker, recipe))
            {
                return 0;
            }
    
            printf("%sBaker %d is leaving fridge 0%s\n", baker->color, baker->id, RESET);

            sem_post(&kitchen.refrigerators[0]);
        } 
        else 
        {
            sem_wait(&kitchen.refrigerators[1]);

            printf("%sBaker %d is entering fridge 1%s\n", baker->color, baker->id, RESET);
    
            if(!check_fridge(baker, recipe))
            {
                return 0;
            }
    
            printf("%sBaker %d is leaving fridge 1%s\n", baker->color, baker->id, RESET);

            sem_post(&kitchen.refrigerators[1]);
        }
 
        sem_wait(&kitchen.pantry);
    
        if(!check_pantry(baker, recipe))
        {
            return 0; 
        } 

        printf("%sBaker %d is leaving the pantry %s\n", baker->color, baker->id, RESET);
        
        sem_post(&kitchen.pantry);
    }
           
    printf("%sBaker %d has aquired all ingredients for %s%s\n", baker->color, baker->id, recipe_names[recipe], RESET);
    return 1;
}

void release_ingredients(Baker* baker, Recipe recipe) 
{
    printf("%sBaker %d is releasing all ingredients for %s%s\n", baker->color, baker->id, recipe_names[recipe], RESET);

    for(int i = 0; recipe_ingredients[recipe][i] != -1; i++) 
    {
        Ingredient ingredient = recipe_ingredients[recipe][i];
        sem_wait(&kitchen.ingredient_locks[ingredient]);
        kitchen.ingredient_available[ingredient]++;
        sem_post(&kitchen.ingredient_locks[ingredient]);
    }
}

int mix_ingredients(Baker* baker, Recipe recipe) 
{
    printf("%sBaker %d is trying to gather all mixing tools%s\n", baker->color, baker->id, RESET);
    
	// Acquire mixer, bowl, and spoon
    sem_wait(&kitchen.mixers);
    sem_wait(&kitchen.bowls);
    sem_wait(&kitchen.spoons);

    int ramsay_trigger = (!ramsay_triggered) && (baker->id == kitchen.ramsay_target) && (rand() % 5 == 0);
    if (ramsay_trigger)
    {
        ramsay_interrupt(baker);
        ramsay_triggered = 1;
        return 0;
    }

   	printf("%sBaker %d is mixing ingredients for %s%s\n", baker->color, baker->id, recipe_names[recipe], RESET);
    
    sleep(1);  // Simulate mixing time

   	// Release mixer, bowl, and spoon
   	sem_post(&kitchen.mixers);
   	sem_post(&kitchen.bowls);
   	sem_post(&kitchen.spoons);

    printf("%sBaker %d has mixed ingredients for %s%s\n", baker->color, baker->id, recipe_names[recipe], RESET);

    return 1;
}

int bake_recipe(Baker* baker, Recipe recipe) 
{
    printf("%sBaker %d is trying to use the oven %s\n", baker->color, baker->id, RESET);
    
	// Acquire oven
   	sem_wait(&kitchen.ovens);

    int ramsay_trigger = (!ramsay_triggered) && (baker->id == kitchen.ramsay_target) && (rand() % 5 == 0);
    if (ramsay_trigger) 
    {
        printf("%s**GORDON RAMSAY INTERRUPTION FOR BAKER %d!**%s\n", baker->color, baker->id, RESET);
        sem_post(&kitchen.ovens);
        printf("%sBaker %d has dropped everything and is restarting%s\n", baker->color, baker->id, RESET);
        ramsay_triggered = 1;
        return 0;
    }

   	printf("%sBaker %d is baking %s%s\n", baker->color, baker->id, recipe_names[recipe], RESET);
    
    sleep(2);  // Simulate baking time

    printf("%sBaker %d completed %s%s\n", baker->color, baker->id, recipe_names[recipe], RESET);

   	// Release oven
   	sem_post(&kitchen.ovens);

    return 1;
}
