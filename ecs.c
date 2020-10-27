#include "ecs.h"

#include <stdio.h>
#include <string.h>

#define OUT_OF_MEMORY "out of memory"
#define NOT_IMPLEMENTED "not implemented"

#define ECS_ABORT(error)                                                       \
  fprintf(stderr, "ABORT %s:%d %s\n", __FILE__, __LINE__, error);              \
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

#define ECS_OFFSET(p, offset) ((void *)(((char *)(p)) + (offset)))

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

static inline void ecs_realloc(void **mem, size_t bytes) {
  *mem = realloc(*mem, bytes);
  ECS_ENSURE(*mem != NULL, OUT_OF_MEMORY);
}

#define MAP_LOAD_FACTOR 0.5
#define MAP_COLLISION_THRESHOLD 30
#define MAP_TOMESTONE ((uint32_t)-1)

typedef struct ecs_bucket_t {
  const void *key;
  uint32_t index;
} ecs_bucket_t;

struct ecs_map_t {
  ecs_hash_fn hash;
  ecs_key_equal_fn key_equal;
  size_t key_size;
  size_t item_size;
  uint32_t count;
  uint32_t load_capacity;
  ecs_bucket_t *sparse;
  uint32_t *reverse_lookup;
  void *dense;
};

ecs_map_t *ecs_map_new(size_t key_size, size_t item_size, ecs_hash_fn hash_fn,
                       ecs_key_equal_fn key_equal_fn, uint32_t capacity) {
  ecs_map_t *map = ecs_malloc(sizeof(ecs_map_t));
  map->hash = hash_fn;
  map->key_equal = key_equal_fn;
  map->sparse = ecs_calloc(sizeof(ecs_bucket_t), capacity);
  map->reverse_lookup =
      ecs_malloc(sizeof(uint32_t) * (capacity * MAP_LOAD_FACTOR + 1));
  map->dense = ecs_malloc(item_size * (capacity * MAP_LOAD_FACTOR + 1));
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
    if (map->key_equal(bucket.key, key) && bucket.index != MAP_TOMESTONE) {
      break;
    }

    i += next_pow_of_2(collisions++);
    bucket = map->sparse[i % map->load_capacity];
  }

  if (bucket.index == 0 || bucket.index == MAP_TOMESTONE) {
    return NULL;
  }

  return ECS_OFFSET(map->dense, map->item_size * bucket.index);
}

static void ecs_map_grow(ecs_map_t *map, float growth_factor) {
  uint32_t new_capacity = map->load_capacity * growth_factor;
  ecs_bucket_t *new_sparse = ecs_calloc(sizeof(ecs_bucket_t), new_capacity);
  free(map->reverse_lookup);
  map->reverse_lookup =
      ecs_malloc(sizeof(uint32_t) * (new_capacity * MAP_LOAD_FACTOR + 1));
  ecs_realloc(&map->dense,
              map->item_size * (new_capacity * MAP_LOAD_FACTOR + 1));

  for (uint32_t i = 0; i < map->load_capacity; i++) {
    ecs_bucket_t bucket = map->sparse[i];

    if (bucket.index != 0 && bucket.index != MAP_TOMESTONE) {
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
    if (map->key_equal(bucket->key, key) && bucket->index != MAP_TOMESTONE) {
      void *loc = ECS_OFFSET(map->dense, map->item_size * bucket->index);
      memcpy(loc, payload, map->item_size);
      return;
    }

    if (!first_tomestone && bucket->index == MAP_TOMESTONE) {
      first_tomestone = bucket;
    }

    i += next_pow_of_2(collisions++);
    ECS_ASSERT(collisions < MAP_COLLISION_THRESHOLD,
               "too many hash collisions");
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

  if (map->count >= map->load_capacity * MAP_LOAD_FACTOR) {
    ecs_map_grow(map, 2);
  }
}

void ecs_map_remove(ecs_map_t *map, const void *key) {
  uint32_t i = map->hash(key);
  ecs_bucket_t bucket = map->sparse[i % map->load_capacity];
  uint32_t next = 0;

  while (bucket.index != 0) {
    if (map->key_equal(bucket.key, key) && bucket.index != MAP_TOMESTONE) {
      break;
    }

    i += next_pow_of_2(next++);
    bucket = map->sparse[i % map->load_capacity];
  }

  if (bucket.index == 0 || bucket.index == MAP_TOMESTONE) {
    return;
  }

  void *tmp = alloca(map->item_size);
  void *left = ECS_OFFSET(map->dense, map->item_size * bucket.index);
  void *right = ECS_OFFSET(map->dense, map->item_size * map->count);
  memcpy(tmp, left, map->item_size);
  memcpy(left, right, map->item_size);
  memcpy(right, tmp, map->item_size);

  map->sparse[map->reverse_lookup[map->count]].index = bucket.index;
  map->sparse[map->reverse_lookup[bucket.index]].index = MAP_TOMESTONE;

  uint32_t reverse_tmp = map->reverse_lookup[bucket.index];
  map->reverse_lookup[bucket.index] = map->reverse_lookup[map->count];
  map->reverse_lookup[map->count] = reverse_tmp;

  map->count--;
}

uint32_t ecs_map_hash_intptr(const void *key) {
  uintptr_t hashed = (uintptr_t)key;
  hashed = ((hashed >> 16) ^ hashed) * 0x45d9f3b;
  hashed = ((hashed >> 16) ^ hashed) * 0x45d9f3b;
  hashed = (hashed >> 16) ^ hashed;
  return hashed;
}

uint32_t ecs_map_hash_string(const void *key) {
  char *str = (char *)key;
  uint32_t hash = 5381;
  char c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

uint32_t ecs_map_hash_type(const void *key) {
  ecs_type_t *type = (ecs_type_t *)key;
  uint32_t hash = 5381;
  ECS_TYPE_EACH(type, e, { hash = ((hash << 5) + hash) + e; });
  return hash;
}

bool ecs_map_equal_intptr(const void *a, const void *b) { return a == b; }

bool ecs_map_equal_string(const void *a, const void *b) {
  return strcmp(a, b) == 0;
}

bool ecs_map_equal_type(const void *a, const void *b) {
  return ecs_type_equal((ecs_type_t *)a, (ecs_type_t *)b);
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
  for (uint32_t i = 0; i < map->load_capacity * MAP_LOAD_FACTOR + 1; i++) {
    if (i == map->count + 1) {
      printf("    -- end of load --\n");
    }

    int item = *(int *)ECS_OFFSET(map->dense, map->item_size * i);
    printf("    %d: %d\n", i, item);
  }
  printf("  ]\n");

  printf("  reverse_lookup: [\n");
  for (uint32_t i = 0; i < map->load_capacity * MAP_LOAD_FACTOR + 1; i++) {
    if (i == map->count + 1) {
      printf("    -- end of load --\n");
    }

    printf("    %d: %d\n", i, map->reverse_lookup[i]);
  }
  printf("  ]\n");

  printf("}\n");
}
#endif // NDEBUG

ecs_type_t *ecs_type_new(uint32_t capacity) {
  ecs_type_t *type = ecs_malloc(sizeof(ecs_type_t));
  type->elements = ecs_malloc(sizeof(ecs_entity_t) * capacity);
  type->capacity = capacity;
  type->count = 0;
  return type;
}

void ecs_type_free(ecs_type_t *type) {
  free(type->elements);
  free(type);
}

ecs_type_t *ecs_type_copy(const ecs_type_t *from) {
  ecs_type_t *type = ecs_malloc(sizeof(ecs_type_t));
  type->elements = ecs_malloc(sizeof(ecs_entity_t) * from->capacity);
  type->capacity = from->capacity;
  type->count = from->count;
  memcpy(type->elements, from->elements, sizeof(ecs_entity_t) * from->count);
  return type;
}

ecs_entity_t *ecs_type_get_array(ecs_type_t *type) { return type->elements; }

uint32_t ecs_type_len(const ecs_type_t *type) { return type->count; }

bool ecs_type_equal(const ecs_type_t *a, const ecs_type_t *b) {
  if (a == b) {
    return true;
  }

  if (a->count != b->count) {
    return false;
  }

  for (uint32_t i = 0; i < a->count; i++) {
    if (a->elements[i] != b->elements[i]) {
      return false;
    }
  }

  return true;
}

int32_t ecs_type_index_of(const ecs_type_t *type, ecs_entity_t e) {
  for (uint32_t i = 0; i < type->count; i++) {
    if (type->elements[i] == e) {
      return i;
    }
  }

  return -1;
}

void ecs_type_add(ecs_type_t *type, ecs_entity_t e) {
  if (type->count == type->capacity) {
    const uint32_t growth = 2;
    ecs_realloc((void **)&type->elements,
                sizeof(ecs_entity_t) * type->capacity * growth);
    type->capacity *= growth;
  }

  uint32_t i = 0;
  while (i < type->count && type->elements[i] < e) {
    i++;
  }

  if (i < type->count && type->elements[i] == e) {
    return;
  }

  ecs_entity_t held = e;
  ecs_entity_t tmp;
  while (i < type->count) {
    tmp = type->elements[i];
    type->elements[i] = held;
    held = tmp;
    i++;
  }

  type->elements[i] = held;
  type->count++;
}

void ecs_type_remove(ecs_type_t *type, ecs_entity_t e) {
  uint32_t i = 0;
  while (i < type->count && type->elements[i] < e) {
    i++;
  }

  if (i == type->count || type->elements[i] != e) {
    return;
  }

  ECS_ASSERT(i < type->count, "index out of bounds");

  while (i < type->count - 1) {
    type->elements[i] = type->elements[i + 1];
    i++;
  }

  type->count--;
}

void ecs_type_inspect(ecs_type_t *type) {
  printf("\ntype: {\n");
  printf("  capacity: %d\n", type->capacity);
  printf("  count: %d\n", type->count);

  printf("  elements: [\n");
  for (uint32_t i = 0; i < type->count; i++) {
    printf("    %d: %lu\n", i, type->elements[i]);
  }
  printf("  ]\n");

  printf("}\n");
}

#define ARCHETYPE_INITIAL_CAPACITY 128

static void
ecs_archetype_resize_component_array(ecs_archetype_t *archetype,
                                     const ecs_map_t *component_index,
                                     uint32_t capacity) {
  uint32_t i = 0;
  ECS_TYPE_EACH(archetype->type, e, {
    size_t *component_size = ecs_map_get(component_index, (void *)e);
    ECS_ENSURE(component_size != NULL, "failed lookup");
    ecs_realloc(&archetype->components[i], sizeof(*component_size) * capacity);
    i++;
  });
}

ecs_archetype_t *ecs_archetype_new(ecs_type_t *type,
                                   const ecs_map_t *component_index) {
  ecs_archetype_t *archetype = ecs_malloc(sizeof(ecs_archetype_t));

  archetype->capacity = ARCHETYPE_INITIAL_CAPACITY;
  archetype->count = 0;
  archetype->type = type;
  archetype->entity_ids =
      ecs_malloc(sizeof(ecs_entity_t) * ARCHETYPE_INITIAL_CAPACITY);
  archetype->components = ecs_calloc(sizeof(void *), ecs_type_len(type));
  archetype->edges = ecs_calloc(sizeof(ecs_edge_t *), 8);

  ecs_archetype_resize_component_array(archetype, component_index,
                                       ARCHETYPE_INITIAL_CAPACITY);
  return archetype;
}

void ecs_archetype_free(ecs_archetype_t *archetype) {
  uint32_t component_count = ecs_type_len(archetype->type);
  for (uint32_t i = 0; i < component_count; i++) {
    free(archetype->components[i]);
  }
  free(archetype->components);

  ecs_type_free(archetype->type);
  free(archetype->entity_ids);
  free(archetype->edges);
  free(archetype);
}

uint32_t ecs_archetype_add(ecs_archetype_t *archetype,
                           ecs_map_t *component_index, ecs_entity_t e) {
  if (archetype->count == archetype->capacity) {
    const uint32_t growth = 2;
    ecs_realloc((void **)&archetype->entity_ids,
                sizeof(ecs_entity_t) * archetype->capacity * growth);
    ecs_archetype_resize_component_array(archetype, component_index,
                                         archetype->capacity * growth);
  }

  archetype->entity_ids[archetype->count] = e;
  return archetype->count++;
}

ecs_registry_t *ecs_init() {
  ecs_registry_t *registry = ecs_malloc(sizeof(ecs_registry_t));
  registry->entity_index = ECS_MAP(intptr, ecs_entity_t, ecs_record_t, 128);
  registry->component_index = ECS_MAP(intptr, ecs_entity_t, size_t, 16);
  registry->named_entities = ECS_MAP(string, char *, ecs_entity_t, 128);
  registry->type_index = ECS_MAP(type, ecs_type_t *, ecs_archetype_t *, 16);
  registry->root =
      ecs_archetype_new(ecs_type_new(0), registry->component_index);
  registry->next_entity_id = 1;
  return registry;
}

void ecs_destroy(ecs_registry_t *registry) {
  ecs_map_free(registry->entity_index);
  ecs_map_free(registry->component_index);
  ecs_map_free(registry->named_entities);
  ecs_map_free(registry->type_index);
  ecs_archetype_free(registry->root);
  free(registry);
}

ecs_entity_t ecs_entity(ecs_registry_t *registry) {
  ecs_archetype_t *root = registry->root;
  ecs_map_set(registry->entity_index, (void *)registry->next_entity_id,
              &(ecs_record_t){root, root->count});
  ecs_archetype_add(root, registry->component_index, registry->next_entity_id);
  return registry->next_entity_id++;
}

ecs_entity_t ecs_component(ecs_registry_t *registry, char *name,
                           size_t component_size) {
  ecs_map_set(registry->component_index, (void *)registry->next_entity_id,
              &(size_t){component_size});
  ecs_name(registry, registry->next_entity_id, name);
  return registry->next_entity_id++;
}

void ecs_name(ecs_registry_t *registry, ecs_entity_t entity, char *name) {
  char err[128];
  sprintf(err, "registering component with existing name: %s", name);
  ECS_ENSURE(ecs_map_get(registry->named_entities, name) == NULL, err);
  ecs_map_set(registry->named_entities, name, &entity);
}
