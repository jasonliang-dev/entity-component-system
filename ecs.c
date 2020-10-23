#include "ecs.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define ECS_OFFSET(p, offset) ((void *)(((char *)(p)) + (offset)))

#define OUT_OF_MEMORY 1
#define TOO_MANY_COLLISIONS 2

static const char *ecs_error_to_str(uint32_t error) {
  switch (error) {
  case OUT_OF_MEMORY:
    return "out of memory";
  case TOO_MANY_COLLISIONS:
    return "too many hash collisions";
  }

  return "unknown error";
}

#define ECS_ABORT(error)                                                       \
  fprintf(stderr, "ABORT %s:%d %s\n", __FILE__, __LINE__,                      \
          ecs_error_to_str(error));                                            \
  abort();

#define ECS_ENSURE(cond, error)                                                \
  if (!(cond)) {                                                               \
    fprintf(stderr, "condition not met: %s\n", #cond);                         \
    ECS_ABORT(error);                                                          \
  }

#ifndef NDEBUG
#define ECS_ASSERT(cond, error) ECS_ENSURE(cond, error);
#else
#define ECS_ASSERT(cond, error)
#endif

static inline void *ecs_malloc(size_t bytes) {
  void *mem = malloc(bytes);
  ECS_ENSURE(mem != NULL, OUT_OF_MEMORY);
  return mem;
}

static inline void *ecs_calloc(size_t items, size_t bytes) {
  void *mem = calloc(items, bytes);
  ECS_ENSURE(mem != NULL, OUT_OF_MEMORY);
  return mem;
}

static inline void *ecs_realloc(void *mem, size_t bytes) {
  mem = realloc(mem, bytes);
  ECS_ENSURE(mem != NULL, OUT_OF_MEMORY);
  return mem;
}

#define LOAD_FACTOR 0.5
#define COLLISION_THRESHOLD 30
#define TOMESTONE ((uint32_t)-1)

struct ecs_bucket_t {
  const void *key;
  uint32_t index;
};

uint32_t ecs_hash_intptr(const void *key) {
  uintptr_t hashed = (uintptr_t)key;
  hashed = ((hashed >> 16) ^ hashed) * 0x45d9f3b;
  hashed = ((hashed >> 16) ^ hashed) * 0x45d9f3b;
  hashed = (hashed >> 16) ^ hashed;
  return hashed;
}

uint32_t ecs_hash_string(const void *key) {
  char *str = (char *)key;
  unsigned long hash = 5381;
  char c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

bool ecs_equal_intptr(const void *a, const void *b) { return a == b; }

bool ecs_equal_string(const void *a, const void *b) {
  return strcmp(a, b) == 0;
}

ecs_map_t *ecs_map_new(size_t key_size, size_t item_size, ecs_hash_fn hash_fn,
                       ecs_key_equal_fn key_equal_fn, uint32_t capacity) {
  ecs_map_t *map = ecs_malloc(sizeof(ecs_map_t));
  map->hash = hash_fn;
  map->key_equal = key_equal_fn;
  map->sparse = ecs_calloc(sizeof(ecs_bucket_t), capacity);
  map->reverse_lookup =
      ecs_malloc(sizeof(uint32_t) * (capacity * LOAD_FACTOR + 1));
  map->dense = ecs_malloc(item_size * (capacity * LOAD_FACTOR + 1));
  map->key_size = key_size;
  map->item_size = item_size;
  map->load_capacity = capacity;
  map->count = 0;
  return map;
}

void ecs_map_free(ecs_map_t *map) {
  free(map->sparse);
  free(map->reverse_lookup);
  free(map->dense);
  free(map);
}

static inline uint32_t next_pow_of_2(uint32_t n) {
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n++;

  return n;
}

void *ecs_map_get(const ecs_map_t *map, const void *key) {
  uint32_t i = map->hash(key);
  ecs_bucket_t bucket = map->sparse[i % map->load_capacity];
  uint32_t collisions = 0;

  while (bucket.index != 0) {
    if (map->key_equal(bucket.key, key) && bucket.index != TOMESTONE) {
      break;
    }

    i += next_pow_of_2(collisions++);
    bucket = map->sparse[i % map->load_capacity];
  }

  if (bucket.index == 0 || bucket.index == TOMESTONE) {
    return NULL;
  }

  return ECS_OFFSET(map->dense, map->item_size * bucket.index);
}

static void grow(ecs_map_t *map, float growth_factor) {
  uint32_t new_capacity = map->load_capacity * growth_factor;
  ecs_bucket_t *new_sparse = ecs_calloc(sizeof(ecs_bucket_t), new_capacity);
  free(map->reverse_lookup);
  map->reverse_lookup =
      ecs_malloc(sizeof(uint32_t) * (new_capacity * LOAD_FACTOR + 1));
  map->dense = ecs_realloc(map->dense,
                           map->item_size * (new_capacity * LOAD_FACTOR + 1));

  for (uint32_t i = 0; i < map->load_capacity; i++) {
    ecs_bucket_t bucket = map->sparse[i];

    if (bucket.index != 0 && bucket.index != TOMESTONE) {
      uint32_t hashed = map->hash(bucket.key);
      ecs_bucket_t *other = &new_sparse[hashed % new_capacity];
      uint32_t collisions = 0;

      while (other->index != 0) {
        hashed += next_pow_of_2(collisions++);
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

void ecs_map_set(ecs_map_t *map, const void *key, const void *payload) {
  uint32_t i = map->hash(key);
  ecs_bucket_t *bucket = &map->sparse[i % map->load_capacity];
  uint32_t collisions = 0;
  ecs_bucket_t *first_tomestone = NULL;

  while (bucket->index != 0) {
    if (map->key_equal(bucket->key, key) && bucket->index != TOMESTONE) {
      void *loc = ECS_OFFSET(map->dense, map->item_size * bucket->index);
      memcpy(loc, payload, map->item_size);
      return;
    }

    if (!first_tomestone && bucket->index == TOMESTONE) {
      first_tomestone = bucket;
    }

    i += next_pow_of_2(collisions++);
    ECS_ASSERT(collisions < COLLISION_THRESHOLD, TOO_MANY_COLLISIONS);
    bucket = &map->sparse[i % map->load_capacity];
  }

  if (first_tomestone) {
    bucket = first_tomestone;
  }

  bucket->key = key;
  bucket->index = map->count + 1;
  void *loc = ECS_OFFSET(map->dense, map->item_size * bucket->index);
  memcpy(loc, payload, map->item_size);
  map->reverse_lookup[bucket->index] = i % map->load_capacity;
  map->count++;

  if (map->count >= map->load_capacity * LOAD_FACTOR) {
    grow(map, 2);
  }
}

void ecs_map_remove(ecs_map_t *map, const void *key) {
  uint32_t i = map->hash(key);
  ecs_bucket_t bucket = map->sparse[i % map->load_capacity];
  uint32_t next = 0;

  while (bucket.index != 0) {
    if (map->key_equal(bucket.key, key) && bucket.index != TOMESTONE) {
      break;
    }

    i += next_pow_of_2(next++);
    bucket = map->sparse[i % map->load_capacity];
  }

  if (bucket.index == 0 || bucket.index == TOMESTONE) {
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

#ifndef NDEBUG
void ecs_map_inspect(ecs_map_t *map) {
  printf("\nmap: {\n"
         "  item_size: %ld bytes\n"
         "  count: %d items\n"
         "  load_capacity: %d\n",
         map->item_size, map->count, map->load_capacity);

  printf("  sparse: [\n");
  for (uint32_t i = 0; i < map->load_capacity; i++) {
    ecs_bucket_t bucket = map->sparse[i];
    printf("    %d: { key: %lu, index: %d }\n", i, (uintptr_t)bucket.key,
           bucket.index);
  }
  printf("  ]\n");

  printf("  dense: [\n");
  for (uint32_t i = 0; i < map->load_capacity * LOAD_FACTOR + 1; i++) {
    if (i == map->count + 1) {
      printf("    -- end of load --\n");
    }

    int item = *(int *)ECS_OFFSET(map->dense, map->item_size * i);
    printf("    %d: %d\n", i, item);
  }
  printf("  ]\n");

  printf("  reverse_lookup: [\n");
  for (uint32_t i = 0; i < map->load_capacity * LOAD_FACTOR + 1; i++) {
    if (i == map->count + 1) {
      printf("    -- end of load --\n");
    }

    printf("    %d: %d\n", i, map->reverse_lookup[i]);
  }
  printf("  ]\n");

  printf("}\n");
}
#endif // NDEBUG

ecs_registry_t *ecs_init() {
  ecs_registry_t *registry = ecs_malloc(sizeof(ecs_registry_t));
  registry->entity_index = ECS_MAP(intptr, ecs_entity_t, ecs_record_t, 64);
  return registry;
}

void ecs_destroy(ecs_registry_t *registry) {
  ecs_map_free(registry->entity_index);
  free(registry);
}
