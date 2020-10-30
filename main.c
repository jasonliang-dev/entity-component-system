#include "ecs.h"
#include "greatest.h"

#include <alloca.h>

TEST map_empty() {
  ecs_map_t *map = ECS_MAP(intptr, int, int, 16);
  ecs_map_free(map);
  PASS();
}

TEST map_set() {
  ecs_map_t *map = ECS_MAP(intptr, int, int, 16);
  int x = 10;
  ecs_map_set(map, (void *)1, &x);
  ecs_map_free(map);
  PASS();
}

TEST map_get() {
  ecs_map_t *map = ECS_MAP(intptr, int, int, 16);
  int x = 10;
  ecs_map_set(map, (void *)1, &x);
  int *val = ecs_map_get(map, (void *)1);
  ASSERT(val != NULL);
  ASSERT_EQ(*val, 10);
  ecs_map_free(map);
  PASS();
}

TEST map_set_multiple() {
  ecs_map_t *map = ECS_MAP(intptr, int, int, 16);
  ecs_map_set(map, (void *)1, &(int){10});
  ecs_map_set(map, (void *)2, &(int){20});
  ASSERT_EQ(*(int *)ecs_map_get(map, (void *)1), 10);
  ASSERT_EQ(*(int *)ecs_map_get(map, (void *)2), 20);
  ecs_map_free(map);
  PASS();
}

TEST map_update() {
  ecs_map_t *map = ECS_MAP(intptr, int, int, 16);
  ecs_map_set(map, (void *)1, &(int){10});
  ecs_map_set(map, (void *)1, &(int){100});
  ASSERT_EQ(*(int *)ecs_map_get(map, (void *)1), 100);
  ecs_map_free(map);
  PASS();
}

TEST map_remove() {
  ecs_map_t *map = ECS_MAP(intptr, int, int, 16);
  ecs_map_set(map, (void *)1, &(int){10});
  ecs_map_remove(map, (void *)1);
  ASSERT_EQ_FMT(NULL, ecs_map_get(map, (void *)1), "%p");
  ecs_map_free(map);
  PASS();
}

TEST map_set_multiple_and_remove() {
  ecs_map_t *map = ECS_MAP(intptr, int, int, 16);
  ecs_map_set(map, (void *)1, &(int){10});
  ecs_map_set(map, (void *)2, &(int){20});
  ecs_map_set(map, (void *)3, &(int){30});
  ecs_map_remove(map, (void *)3);
  ASSERT_EQ(*(int *)ecs_map_get(map, (void *)1), 10);
  ASSERT_EQ(ecs_map_get(map, (void *)3), NULL);
  ecs_map_free(map);
  PASS();
}

TEST map_set_a_lot(const int count) {
  ecs_map_t *map = ECS_MAP(intptr, int, int, 16);

  for (int i = 1; i < count; i++) {
    ecs_map_set(map, (void *)(uintptr_t)i, &(int){i * 10});
  }

  for (int i = 1; i < count; i++) {
    ASSERT_EQ(*(int *)ecs_map_get(map, (void *)(uintptr_t)i), i * 10);
  }

  ecs_map_free(map);
  PASS();
}

TEST map_remove_a_lot(int count) {
  ecs_map_t *map = ECS_MAP(intptr, int, int, 16);

  for (int i = 1; i < count; i++) {
    ecs_map_set(map, (void *)(uintptr_t)i, &(int){i * 10});
  }

  for (int i = 1; i + 1 < count / 2; i += 2) {
    ecs_map_remove(map, (void *)(uintptr_t)i);
  }

  for (int i = 1; i + 1 < count / 2; i += 2) {
    ASSERT_EQ_FMT(NULL, ecs_map_get(map, (void *)(uintptr_t)i), "%p");
    ASSERT_EQ(*(int *)ecs_map_get(map, (void *)(uintptr_t)(i + 1)),
              (i + 1) * 10);
  }

  ecs_map_free(map);
  PASS();
}

TEST map_string_keys() {
  ecs_map_t *map = ECS_MAP(string, char *, int, 16);
  ecs_map_set(map, "foo", &(int){10});
  ecs_map_set(map, "bar", &(int){20});
  ASSERT_EQ(*(int *)ecs_map_get(map, "foo"), 10);
  ASSERT_EQ(*(int *)ecs_map_get(map, "bar"), 20);
  ASSERT_EQ(NULL, ecs_map_get(map, "baz"));
  char *bar = alloca(4);
  memcpy(bar, "bar", 4);
  ecs_map_remove(map, bar);
  ASSERT_EQ(NULL, ecs_map_get(map, "bar"));
  ecs_map_free(map);
  PASS();
}

TEST map_string_keys_struct_values() {
  struct person_t {
    char *name;
    int age;
    char *hobby;
  };

  ecs_map_t *map = ECS_MAP(string, char *, struct person_t, 16);
  ecs_map_set(map, "jason", &(struct person_t){"Jason", 20, "Playing guitar"});
  ecs_map_set(map, "june",
              &(struct person_t){"June", 24, "Listening to music"});
  struct person_t *jason = ecs_map_get(map, "jason");
  struct person_t *june = ecs_map_get(map, "june");
  struct person_t *unknown = ecs_map_get(map, "foobarbaz");
  ASSERT_STR_EQ(jason->name, "Jason");
  ASSERT_EQ(jason->age, 20);
  ASSERT_STR_EQ(jason->hobby, "Playing guitar");
  ASSERT_STR_EQ(june->name, "June");
  ASSERT_EQ(june->age, 24);
  ASSERT_STR_EQ(june->hobby, "Listening to music");
  ASSERT_EQ(unknown, NULL);
  ecs_map_free(map);
  PASS();
}

SUITE(map) {
  RUN_TEST(map_empty);
  RUN_TEST(map_set);
  RUN_TEST(map_get);
  RUN_TEST(map_set_multiple);
  RUN_TEST(map_update);
  RUN_TEST(map_remove);
  RUN_TEST(map_set_multiple_and_remove);
  for (int i = 10; i <= 100000; i *= 10) {
    RUN_TEST1(map_set_a_lot, i);
    RUN_TEST1(map_remove_a_lot, i);
  }
  RUN_TEST(map_string_keys);
  RUN_TEST(map_string_keys_struct_values);
}

TEST type_empty() {
  ecs_type_t *type = ecs_type_new(8);
  ecs_type_free(type);
  PASS();
}

TEST type_contains() {
  ecs_type_t *type = ecs_type_new(8);
  ASSERT_EQ(ecs_type_index_of(type, 1), -1);
  ecs_type_free(type);
  PASS();
}

TEST type_add_1() {
  ecs_type_t *type = ecs_type_new(8);
  ecs_type_add(type, 1);
  ASSERT_EQ(ecs_type_index_of(type, 1), 0);
  ecs_type_free(type);
  PASS();
}

TEST type_add_multiple(ecs_entity_t count) {
  ecs_type_t *type = ecs_type_new(16);
  for (ecs_entity_t i = 0; i < count; i++) {
    ecs_type_add(type, i + 1);
  }

  for (ecs_entity_t i = 0; i < count; i++) {
    ASSERT_EQ(ecs_type_index_of(type, i + 1), (int)i);
  }
  ASSERT_EQ(ecs_type_index_of(type, 0), -1);
  ecs_type_free(type);
  PASS();
}

TEST type_add_multiple_reversed(ecs_entity_t count) {
  ecs_type_t *type = ecs_type_new(16);
  for (ecs_entity_t i = 0; i < count; i++) {
    ecs_type_add(type, count - i);
  }

  for (ecs_entity_t i = 0; i < count; i++) {
    ASSERT(ecs_type_index_of(type, count - i) != -1);
  }
  ASSERT_EQ(ecs_type_index_of(type, 0), -1);
  ecs_type_free(type);
  PASS();
}

TEST type_add_multiple_random(ecs_entity_t max) {
  ecs_type_t *type = ecs_type_new(16);
  ecs_entity_t ran = rand() % max;
  for (ecs_entity_t i = 0; i < ran; i++) {
    ecs_type_add(type, rand());
  }

  ecs_type_free(type);
  PASS();
}

TEST type_add_duplicate() {
  ecs_type_t *type = ecs_type_new(8);
  ecs_type_add(type, 1);
  ecs_type_add(type, 1);
  ASSERT_EQ(ecs_type_index_of(type, 1), 0);
  ecs_type_free(type);
  PASS();
}

TEST type_remove_from_empty() {
  ecs_type_t *type = ecs_type_new(8);
  ecs_type_remove(type, 1);
  ASSERT_EQ(ecs_type_index_of(type, 1), -1);
  ecs_type_free(type);
  PASS();
}

TEST type_remove_from_1() {
  ecs_type_t *type = ecs_type_new(8);
  ecs_type_add(type, 1);
  ecs_type_remove(type, 1);
  ASSERT_EQ(ecs_type_index_of(type, 1), -1);
  ecs_type_free(type);
  PASS();
}

TEST type_remove_from_many() {
  ecs_type_t *type = ecs_type_new(8);
  ecs_type_add(type, 3);
  ecs_type_add(type, 2);
  ecs_type_add(type, 5);
  ecs_type_remove(type, 2);
  ecs_type_add(type, 1);
  ASSERT_EQ(ecs_type_index_of(type, 2), -1);
  ASSERT(ecs_type_index_of(type, 3) != -1);
  ASSERT(ecs_type_index_of(type, 5) != -1);
  ecs_type_free(type);
  PASS();
}

TEST type_equal() {
  ecs_type_t *a = ecs_type_new(8);
  ecs_type_add(a, 1);
  ecs_type_add(a, 2);
  ecs_type_add(a, 3);
  ecs_type_t *b = ecs_type_new(8);
  ecs_type_add(b, 3);
  ecs_type_add(b, 1);
  ecs_type_add(b, 2);
  ASSERT(ecs_type_equal(a, a));
  ASSERT(ecs_type_equal(b, b));
  ASSERT(ecs_type_equal(a, b));
  ecs_type_free(a);
  ecs_type_free(b);
  PASS();
}

TEST type_copy() {
  ecs_type_t *a = ecs_type_new(8);
  ecs_type_add(a, 1);
  ecs_type_add(a, 2);
  ecs_type_add(a, 3);
  ecs_type_t *b = ecs_type_copy(a);
  ASSERT(ecs_type_equal(a, b));
  ecs_type_remove(b, 1);
  ASSERT_FALSE(ecs_type_equal(a, b));
  ecs_type_free(a);
  ecs_type_free(b);
  PASS();
}

TEST type_superset() {
  ecs_type_t *a = ecs_type_new(8);
  ecs_type_add(a, 1);
  ecs_type_add(a, 2);
  ecs_type_add(a, 3);
  ecs_type_t *b = ecs_type_copy(a);
  ecs_type_add(b, 5);
  ecs_type_add(b, 6);
  ecs_type_add(b, 7);
  ASSERT(ecs_type_is_superset(b, a));
  ecs_type_free(a);
  ecs_type_free(b);
  PASS();
}

SUITE(type) {
  RUN_TEST(type_empty);
  RUN_TEST(type_contains);
  RUN_TEST(type_add_1);
  for (int i = 10; i <= 1000; i *= 10) {
    RUN_TEST1(type_add_multiple, i);
    RUN_TEST1(type_add_multiple_reversed, i);
    RUN_TEST1(type_add_multiple_random, i);
  }
  RUN_TEST(type_add_duplicate);
  RUN_TEST(type_remove_from_empty);
  RUN_TEST(type_remove_from_1);
  RUN_TEST(type_remove_from_many);
  RUN_TEST(type_equal);
  RUN_TEST(type_copy);
  // RUN_TEST(type_superset);
  (void)type_superset;
}

TEST ecs_minimal() {
  ecs_registry_t *registry = ecs_init();
  ecs_destroy(registry);
  PASS();
}

TEST ecs_register() {
  ecs_registry_t *registry = ecs_init();
  ECS_COMPONENT(registry, int);
  ecs_destroy(registry);
  PASS();
}

TEST ecs_create_entity() {
  ecs_registry_t *registry = ecs_init();
  ecs_entity_t e = ecs_entity(registry);
  (void)e;
  ecs_destroy(registry);
  PASS();
}

TEST ecs_attach_component() {
  ecs_registry_t *registry = ecs_init();
  ecs_entity_t int_component = ECS_COMPONENT(registry, int);
  ecs_entity_t e = ecs_entity(registry);
  ecs_attach(registry, e, int_component);
  ecs_destroy(registry);
  PASS();
}

TEST ecs_set_component_data() {
  ecs_registry_t *registry = ecs_init();
  ecs_entity_t int_component = ECS_COMPONENT(registry, int);
  ecs_entity_t e = ecs_entity(registry);
  ecs_attach(registry, e, int_component);
  ecs_set(registry, e, int_component, &(int){1});
  ecs_destroy(registry);
  PASS();
}

void print(ecs_view_t view, unsigned int row) {
  int *x = ecs_view(view, row, 0);
  printf("x is: %d\n", *x);
}

TEST ecs_run_system() {
  ecs_registry_t *registry = ecs_init();
  ecs_entity_t int_component = ECS_COMPONENT(registry, int);
  ecs_entity_t e = ecs_entity(registry);
  ecs_attach(registry, e, int_component);
  ecs_set(registry, e, int_component, &(int){0});
  ecs_signature_t *sig = ecs_signature_new_n(1, int_component);
  ecs_system(registry, sig, print);
  ecs_step(registry);
  ecs_destroy(registry);
  PASS();
}

void move(ecs_view_t view, unsigned int row) {
  int *p = ecs_view(view, row, 0);
  int *v = ecs_view(view, row, 1);
  *p += *v;
  printf("p is: %d\n", *p);
}

TEST ecs_run_system_loop() {
  typedef int pos;
  typedef int vel;

  ecs_registry_t *registry = ecs_init();
  ecs_entity_t pos_component = ECS_COMPONENT(registry, pos);
  ecs_entity_t vel_component = ECS_COMPONENT(registry, vel);
  ecs_entity_t e = ecs_entity(registry);
  ecs_attach(registry, e, pos_component);
  ecs_attach(registry, e, vel_component);
  ecs_set(registry, e, pos_component, &(pos){0});
  ecs_set(registry, e, vel_component, &(pos){1});
  ECS_SYSTEM(registry, move, 2, pos_component, vel_component);

  for (int i = 0; i < 15; i++) {
    ecs_step(registry);
  }

  ecs_destroy(registry);
  PASS();
}

typedef struct Position {
  float x;
  float y;
} Position;

typedef struct Velocity {
  float x;
  float y;
} Velocity;

void do_ecs_move(ecs_view_t view, unsigned int row) {
  Position *p = (Position *)ecs_view(view, row, 0);
  Velocity *v = (Velocity *)ecs_view(view, row, 1);
  p->x += v->x;
  p->y += v->y;
}

TEST ecs_from_bench(int context[]) {
  ecs_registry_t *registry = ecs_init();

  int entities = context[0];
  int iterations = context[1];

  const ecs_entity_t pos_component = ECS_COMPONENT(registry, Position);
  const ecs_entity_t vel_component = ECS_COMPONENT(registry, Velocity);

  for (int i = 0; i < entities; i++) {
    ecs_entity_t e = ecs_entity(registry);
    ecs_attach(registry, e, pos_component);
    ecs_attach(registry, e, vel_component);
    Position initial_position = {0, 0};
    Velocity initial_velocity = {1, 1};
    ecs_set(registry, e, pos_component, &initial_position);
    ecs_set(registry, e, vel_component, &initial_velocity);
  }

  ECS_SYSTEM(registry, do_ecs_move, 2, pos_component, vel_component);

  for (int i = 0; i < iterations; i++) {
    ecs_step(registry);
  }

  ecs_destroy(registry);
  PASS();
}

SUITE(ecs) {
  RUN_TEST1(ecs_from_bench, ((int[2]){1000, 1000}));
  RUN_TEST(ecs_run_system_loop);
  RUN_TEST(ecs_run_system);
  RUN_TEST(ecs_minimal);
  RUN_TEST(ecs_register);
  RUN_TEST(ecs_create_entity);
  RUN_TEST(ecs_attach_component);
  RUN_TEST(ecs_set_component_data);
}

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[]) {
  srand(time(NULL));

  GREATEST_MAIN_BEGIN();
  RUN_SUITE(map);
  RUN_SUITE(type);
  RUN_SUITE(ecs);
  GREATEST_MAIN_END();
}
