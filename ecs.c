#include "ecs.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define LOAD_FACTOR 0.5
#define TOMESTONE ((uint32_t)-1)

#define ECS_OFFSET(p, offset) ((void *)(((char *)(p)) + (offset)))

#define ECS_NEW(T, count) ((T *)ecs_malloc((count) * sizeof(T)))

static inline void *ecs_malloc(size_t bytes)
{
    void *mem = malloc(bytes);
    assert(mem != NULL);
    return mem;
}

static inline void *ecs_calloc(size_t items, size_t bytes)
{
    void *mem = calloc(items, bytes);
    assert(mem != NULL);
    return mem;
}

static inline void *ecs_realloc(void *mem, size_t bytes)
{
    mem = realloc(mem, bytes);
    assert(mem != NULL);
    return mem;
}

struct ecs_bucket_t
{
    const void *key;
    uint32_t index;
};

uint32_t ecs_hash_int(const ecs_map_t *map, const void *key)
{
    (void)map;
    uint32_t hashed = *(uint32_t *)key;
    hashed = ((hashed >> 16) ^ hashed) * 0x45d9f3b;
    hashed = ((hashed >> 16) ^ hashed) * 0x45d9f3b;
    hashed = (hashed >> 16) ^ hashed;
    return hashed;
}

uint32_t ecs_hash_string(const ecs_map_t *map, const void *key)
{
    (void)map;
    char *str = (char *)key;
    unsigned long hash = 5381;
    char c;

    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

uint32_t ecs_hash_direct(const ecs_map_t *map, const void *key)
{
    const uintptr_t shift = log2(1 + sizeof(map->key_size));
    return (uintptr_t)key >> shift;
}

bool ecs_equal_string(const void *a, const void *b)
{
    return strcmp(a, b) == 0;
}

bool ecs_equal_direct(const void *a, const void *b)
{
    return a == b;
}

ecs_map_t *ecs_map_new(size_t key_size, size_t item_size, ecs_hash_fn hash_fn, ecs_key_equal_fn key_equal_fn,
                       uint32_t capacity)
{
    ecs_map_t *map = ecs_malloc(sizeof(ecs_map_t));
    map->hash = hash_fn;
    map->key_equal = key_equal_fn;
    map->sparse = ecs_calloc(sizeof(ecs_bucket_t), capacity);
    map->reverse_lookup = ecs_malloc(sizeof(uint32_t) * (capacity * LOAD_FACTOR + 1));
    map->dense = ecs_malloc(item_size * (capacity * LOAD_FACTOR + 1));
    map->key_size = key_size;
    map->item_size = item_size;
    map->load_capacity = capacity;
    map->count = 0;
    return map;
}

void ecs_map_free(ecs_map_t *map)
{
    free(map->sparse);
    free(map->reverse_lookup);
    free(map->dense);
    free(map);
}

static inline uint32_t next_pow_of_2(uint32_t n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;

    return n;
}

void *ecs_map_get(const ecs_map_t *map, const void *key)
{
    uint32_t i = map->hash(map, key);
    ecs_bucket_t bucket = map->sparse[i % map->load_capacity];
    uint32_t next = 0;

    while (bucket.index != 0)
    {
        if (map->key_equal(bucket.key, key) && bucket.index != TOMESTONE)
        {
            break;
        }

        i += next_pow_of_2(next++);
        bucket = map->sparse[i % map->load_capacity];
    }

    if (bucket.index == 0 || bucket.index == TOMESTONE)
    {
        return NULL;
    }

    return ECS_OFFSET(map->dense, map->item_size * bucket.index);
}

static void grow(ecs_map_t *map, float growth_factor)
{
    uint32_t new_capacity = map->load_capacity * growth_factor;
    ecs_bucket_t *new_sparse = ecs_calloc(sizeof(ecs_bucket_t), new_capacity);
    free(map->reverse_lookup);
    map->reverse_lookup = ecs_malloc(sizeof(uint32_t) * (new_capacity * LOAD_FACTOR + 1));
    map->dense = ecs_realloc(map->dense, map->item_size * (new_capacity * LOAD_FACTOR + 1));

    for (uint32_t i = 0; i < map->load_capacity; i++)
    {
        ecs_bucket_t bucket = map->sparse[i];

        if (bucket.index != 0 && bucket.index != TOMESTONE)
        {
            uint32_t hashed = map->hash(map, bucket.key);
            ecs_bucket_t *other = &new_sparse[hashed % new_capacity];
            uint32_t next = 0;

            while (other->index != 0)
            {
                hashed += next_pow_of_2(next++);
                other = &new_sparse[hashed % new_capacity];
            }

            other->key = bucket.key;
            other->index = bucket.index;
            map->reverse_lookup[bucket.index] = hashed % new_capacity;
        }
    }

    free(map->sparse);
    map->sparse = new_sparse;
    map->load_capacity = new_capacity;
}

void ecs_map_set(ecs_map_t *map, const void *key, const void *payload)
{
    uint32_t i = map->hash(map, key);
    ecs_bucket_t *bucket = &map->sparse[i % map->load_capacity];
    uint32_t next = 0;
    ecs_bucket_t *first_tomestone = NULL;

    while (bucket->index != 0)
    {
        if (map->key_equal(bucket->key, key) && bucket->index != TOMESTONE)
        {
            void *loc = ECS_OFFSET(map->dense, map->item_size * bucket->index);
            memcpy(loc, payload, map->item_size);
            return;
        }

        if (!first_tomestone && bucket->index == TOMESTONE)
        {
            first_tomestone = bucket;
        }

        i += next_pow_of_2(next++);
        bucket = &map->sparse[i % map->load_capacity];
    }

    if (first_tomestone)
    {
        bucket = first_tomestone;
    }

    bucket->key = key;
    bucket->index = map->count + 1;
    void *loc = ECS_OFFSET(map->dense, map->item_size * bucket->index);
    memcpy(loc, payload, map->item_size);
    map->reverse_lookup[bucket->index] = i % map->load_capacity;
    map->count++;

    if (map->count >= map->load_capacity * LOAD_FACTOR)
    {
        grow(map, 2);
    }
}

void ecs_map_remove(ecs_map_t *map, const void *key)
{
    uint32_t i = map->hash(map, key);
    ecs_bucket_t bucket = map->sparse[i % map->load_capacity];
    uint32_t next = 0;

    while (bucket.index != 0)
    {
        if (map->key_equal(bucket.key, key) && bucket.index != TOMESTONE)
        {
            break;
        }

        i += next_pow_of_2(next++);
        bucket = map->sparse[i % map->load_capacity];
    }

    if (bucket.index == 0 || bucket.index == TOMESTONE)
    {
        return;
    }

    void *tmp = alloca(map->item_size);
    void *left = ECS_OFFSET(map->dense, map->item_size * bucket.index);
    void *right = ECS_OFFSET(map->dense, map->item_size * map->count);
    memcpy(tmp, left, map->item_size);
    memcpy(left, right, map->item_size);
    memcpy(right, tmp, map->item_size);

    map->sparse[map->reverse_lookup[map->count]].index = bucket.index;
    map->sparse[map->reverse_lookup[bucket.index]].index = TOMESTONE;

    uint32_t reverse_tmp = map->reverse_lookup[bucket.index];
    map->reverse_lookup[bucket.index] = map->reverse_lookup[map->count];
    map->reverse_lookup[map->count] = reverse_tmp;

    map->count--;
}

void ecs_map_inspect(ecs_map_t *map)
{
    printf("\nmap: {\n"
           "  item_size: %ld bytes\n"
           "  count: %d items\n"
           "  load_capacity: %d\n",
           map->item_size, map->count, map->load_capacity);

    /*
    printf("  sparse: [\n");
    for (uint32_t i = 0; i < map->load_capacity; i++)
    {
        ecs_bucket_t bucket = map->sparse[i];
        printf("    %d: { key: %d, index: %d }\n", i, bucket.key, bucket.index);
        printf("    %d: { key: ", i);
        if (map->hash_func == hash_int)
        {
            printf("%d", *(uint32_t *)bucket.key);
        }
        printf("    %d: { key: %d, index: %d }\n", i, bucket.key, bucket.index);
    }
    printf("  ]\n");
    */

    printf("  dense: [\n");
    for (uint32_t i = 0; i < map->load_capacity * LOAD_FACTOR + 1; i++)
    {
        if (i == map->count + 1)
        {
            printf("    -- end of load --\n");
        }

        int item = *(int *)ECS_OFFSET(map->dense, map->item_size * i);
        printf("    %d: %d\n", i, item);
    }
    printf("  ]\n");

    printf("  reverse_lookup: [\n");
    for (uint32_t i = 0; i < map->load_capacity * LOAD_FACTOR + 1; i++)
    {
        if (i == map->count + 1)
        {
            printf("    -- end of load --\n");
        }

        printf("    %d: %d\n", i, map->reverse_lookup[i]);
    }
    printf("  ]\n");

    printf("}\n");
}

ecs_registry_t *ecs_init()
{
    ecs_registry_t *registry = ecs_malloc(sizeof(ecs_registry_t));
    registry->entity_index =
        ecs_map_new(sizeof(ecs_entity_t), sizeof(ecs_record_t), ecs_hash_direct, ecs_equal_direct, 64);
    return registry;
}

void ecs_destroy(ecs_registry_t *registry)
{
    ecs_map_free(registry->entity_index);
    free(registry);
}
