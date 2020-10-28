#include "ecs.h"

#include <stdio.h>
#include <string.h>

#define OUT_OF_MEMORY "out of memory"
#define OUT_OF_BOUNDS "index out of bounds"
#define FAILED_LOOKUP "lookup failed and returned null"
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
  if (bytes != 0) {
    ECS_ENSURE(*mem != NULL, OUT_OF_MEMORY);
  }
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

void *ecs_map_values(ecs_map_t *map) {
  return ECS_OFFSET(map->dense, map->item_size);
}

uint32_t ecs_map_len(ecs_map_t *map) { return map->count; }

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
    if (type->capacity == 0) {
      type->capacity = 1;
    }

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

  ECS_ASSERT(i < type->count, OUT_OF_BOUNDS);

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

ecs_edge_list_t *ecs_edge_list_new() {
  ecs_edge_list_t *edge_list = ecs_malloc(sizeof(ecs_edge_list_t));
  edge_list->capacity = 8;
  edge_list->count = 0;
  edge_list->edges = ecs_malloc(sizeof(ecs_edge_t) * edge_list->capacity);
  return edge_list;
}

void ecs_edge_list_free(ecs_edge_list_t *edge_list) {
  free(edge_list->edges);
  free(edge_list);
}

uint32_t ecs_edge_list_len(const ecs_edge_list_t *edge_list) {
  return edge_list->count;
}

void ecs_edge_list_add(ecs_edge_list_t *edge_list, ecs_edge_t edge) {
  if (edge_list->count == edge_list->capacity) {
    const uint32_t growth = 2;
    ecs_realloc((void **)&edge_list->edges,
                sizeof(ecs_edge_t) * edge_list->capacity * growth);
  }

  edge_list->edges[edge_list->count++] = edge;
}

void ecs_edge_list_remove(ecs_edge_list_t *edge_list, ecs_entity_t component) {
  ecs_edge_t *edges = edge_list->edges;

  uint32_t i = 0;
  while (i < edge_list->count && edges[i].component != component) {
    i++;
  }

  if (i == edge_list->count) {
    return;
  }

  ecs_edge_t tmp = edges[i];
  edges[i] = edges[edge_list->count];
  edges[edge_list->count--] = tmp;
}

ecs_edge_t *ecs_edge_list_find(ecs_edge_list_t *edge_list,
                               ecs_entity_t component) {
  for (uint32_t i = 0; i < edge_list->count; i++) {
    if (edge_list->edges[i].component == component) {
      return &edge_list->edges[i];
    }
  }

  return NULL;
}

#define ARCHETYPE_INITIAL_CAPACITY 16

static void
ecs_archetype_resize_component_array(ecs_archetype_t *archetype,
                                     const ecs_map_t *component_index,
                                     uint32_t capacity) {
  uint32_t i = 0;
  ECS_TYPE_EACH(archetype->type, e, {
    size_t *component_size = ecs_map_get(component_index, (void *)e);
    ECS_ASSERT(component_size != NULL, FAILED_LOOKUP);
    ecs_realloc(&archetype->components[i], sizeof(*component_size) * capacity);
    i++;
  });
}

ecs_archetype_t *ecs_archetype_new(ecs_type_t *type,
                                   const ecs_map_t *component_index,
                                   ecs_map_t *type_index) {
  ECS_ENSURE(ecs_map_get(type_index, type) == NULL, "archetype already exists");

  ecs_archetype_t *archetype = ecs_malloc(sizeof(ecs_archetype_t));

  archetype->capacity = ARCHETYPE_INITIAL_CAPACITY;
  archetype->count = 0;
  archetype->type = type;
  archetype->entity_ids =
      ecs_malloc(sizeof(ecs_entity_t) * ARCHETYPE_INITIAL_CAPACITY);
  archetype->components = ecs_calloc(sizeof(void *), ecs_type_len(type));
  archetype->edges = ecs_edge_list_new();

  ecs_archetype_resize_component_array(archetype, component_index,
                                       ARCHETYPE_INITIAL_CAPACITY);
  ecs_map_set(type_index, type, &archetype);

  return archetype;
}

void ecs_archetype_free(ecs_archetype_t *archetype) {
  uint32_t component_count = ecs_type_len(archetype->type);
  for (uint32_t i = 0; i < component_count; i++) {
    free(archetype->components[i]);
  }
  free(archetype->components);

  ecs_type_free(archetype->type);
  ecs_edge_list_free(archetype->edges);
  free(archetype->entity_ids);
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

ecs_entity_t ecs_archetype_remove(ecs_archetype_t *archetype,
                                  ecs_map_t *component_index, uint32_t row) {
  ECS_ENSURE(row < archetype->count, OUT_OF_BOUNDS);
  ecs_entity_t removed = archetype->entity_ids[row];
  archetype->entity_ids[row] = archetype->entity_ids[archetype->count - 1];

  uint32_t i = 0;
  ECS_TYPE_EACH(archetype->type, e, {
    size_t *component_size = ecs_map_get(component_index, (void *)e);
    ECS_ASSERT(component_size != NULL, FAILED_LOOKUP);
    void *left = ECS_OFFSET(archetype->components, *component_size * i);
    void *right = ECS_OFFSET(archetype->components,
                             *component_size * (archetype->count - 1));
    memcpy(left, right, *component_size);
    i++;
  });

  archetype->count--;
  return removed;
}

#ifndef NDEBUG
void ecs_archetype_inspect(ecs_archetype_t *archetype) {
  printf("\narchetype: {\n");
  printf("  self: %p\n", (void *)archetype);
  printf("  capacity: %d\n", archetype->capacity);
  printf("  count: %d\n", archetype->count);

  printf("  type: %p\n", (void *)archetype->type);
  printf("  type: [ ");
  ECS_TYPE_EACH(archetype->type, e, { printf("%lu ", e); });
  printf("]\n");

  printf("  entity_ids: [\n");
  for (uint32_t i = 0; i < archetype->count; i++) {
    printf("    %lu\n", archetype->entity_ids[i]);
  }
  printf("  ]\n");

  printf("  edges: [\n");
  ECS_EDGE_LIST_EACH(archetype->edges, edge, {
    printf("    { %lu, add = %p, remove = %p }\n", edge.component,
           (void *)edge.add, (void *)edge.remove);
  });
  printf("  ]\n");

  printf("}\n");
}
#endif

ecs_registry_t *ecs_init() {
  ecs_registry_t *registry = ecs_malloc(sizeof(ecs_registry_t));
  registry->entity_index = ECS_MAP(intptr, ecs_entity_t, ecs_record_t, 16);
  registry->component_index = ECS_MAP(intptr, ecs_entity_t, size_t, 16);
  registry->type_index = ECS_MAP(type, ecs_type_t *, ecs_archetype_t *, 16);
  registry->named_entities = ECS_MAP(string, char *, ecs_entity_t, 16);
  ecs_type_t *root_type = ecs_type_new(0);
  registry->root = ecs_archetype_new(root_type, registry->component_index,
                                     registry->type_index);
  registry->next_entity_id = 1;
  return registry;
}

void ecs_destroy(ecs_registry_t *registry) {
  ECS_MAP_VALUES_EACH(registry->type_index, ecs_archetype_t *, archetype,
                      { ecs_archetype_free(*archetype); });
  ecs_map_free(registry->entity_index);
  ecs_map_free(registry->component_index);
  ecs_map_free(registry->type_index);
  ecs_map_free(registry->named_entities);
  free(registry);
}

ecs_entity_t ecs_entity(ecs_registry_t *registry) {
  ecs_archetype_t *root = registry->root;
  uint32_t row = ecs_archetype_add(root, registry->component_index,
                                   registry->next_entity_id);
  ecs_map_set(registry->entity_index, (void *)registry->next_entity_id,
              &(ecs_record_t){root, row});
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
  sprintf(err, "registering component with duplicate name: %s", name);
  ECS_ENSURE(ecs_map_get(registry->named_entities, name) == NULL, err);
  ecs_map_set(registry->named_entities, name, &entity);
}

void ecs_attach(ecs_registry_t *registry, ecs_entity_t entity,
                ecs_entity_t component) {
  ecs_record_t *record = ecs_map_get(registry->entity_index, (void *)entity);

  if (record == NULL) {
    char err[255];
    char *name = ecs_map_get(registry->named_entities, (void *)component);
    if (name == NULL) {
      sprintf(err, "attaching unknown component %lu to unknown entity %lu",
              component, entity);
    } else {
      sprintf(err, "attaching component %lu (aka '%s') to unknown entity %lu",
              component, name, entity);
    }

    ECS_ABORT(err);
  }

  ecs_type_t *init_type = record->archetype->type;
  ecs_type_t *fini_type = ecs_type_copy(init_type);
  ecs_type_add(fini_type, component);

  ecs_archetype_t **fini_archetype_dp =
      ecs_map_get(registry->type_index, fini_type);
  ecs_archetype_t *fini_archetype;

  if (fini_archetype_dp == NULL) {
    fini_archetype = ecs_archetype_new(fini_type, registry->component_index,
                                        registry->type_index);
    ecs_edge_t edge = {component, .add = fini_archetype,
                       .remove = record->archetype};
    ecs_edge_list_add(record->archetype->edges, edge);
    ecs_edge_list_add(fini_archetype->edges, edge);
  } else {
    ecs_type_free(fini_type);
    fini_archetype = *fini_archetype_dp;
  }

  ecs_entity_t removed = ecs_archetype_remove(
      record->archetype, registry->component_index, record->row);
  ECS_ASSERT(removed == entity, "entity record mismatch");
  uint32_t row =
      ecs_archetype_add(fini_archetype, registry->component_index, entity);
  ecs_map_set(registry->entity_index, (void *)entity,
              &(ecs_record_t){fini_archetype, row});
}

void ecs_attach_w_name(ecs_registry_t *registry, ecs_entity_t entity,
                       char *component_name) {
  ecs_entity_t *component =
      ecs_map_get(registry->named_entities, component_name);
  ECS_ENSURE(component != NULL, FAILED_LOOKUP);
  ecs_attach(registry, entity, *component);
}

void ecs_set(ecs_registry_t *registry, ecs_entity_t entity,
             ecs_entity_t component, const void *data) {
  size_t *component_size =
      ecs_map_get(registry->component_index, (void *)component);
  ECS_ENSURE(component_size != NULL, FAILED_LOOKUP);
  ecs_record_t *record = ecs_map_get(registry->entity_index, (void *)entity);
  ECS_ENSURE(record != NULL, FAILED_LOOKUP);
  int32_t column = ecs_type_index_of(record->archetype->type, component);
  ECS_ENSURE(column != -1, OUT_OF_BOUNDS);

  void *component_array = record->archetype->components[column];
  void *element = ECS_OFFSET(component_array, *component_size * record->row);
  memcpy(element, data, *component_size);
}
