#include "ecs.h"
#include "greatest.h"

TEST map_empty()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    ecs_map_free(map);
    PASS();
}

TEST map_set()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    int x = 10;
    ecs_map_set(map, 1, &x);
    ecs_map_free(map);
    PASS();
}

TEST map_get()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    int x = 10;
    ecs_map_set(map, 1, &x);
    int *val = ecs_map_get(map, 1);
    ASSERT(val != NULL);
    ASSERT_EQ(*val, 10);
    ecs_map_free(map);
    PASS();
}

TEST map_set_multiple()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    ecs_map_set(map, 1, &(int){10});
    ecs_map_set(map, 2, &(int){20});
    ASSERT_EQ(*(int *)ecs_map_get(map, 1), 10);
    ASSERT_EQ(*(int *)ecs_map_get(map, 2), 20);
    ecs_map_inspect(map);
    ecs_map_free(map);
    PASS();
}

TEST map_update()
{
    ecs_map_t *map = ecs_map_new(sizeof(int), 10);
    ecs_map_set(map, 1, &(int){10});
    ecs_map_set(map, 1, &(int){100});
    ASSERT_EQ(*(int *)ecs_map_get(map, 1), 100);
    ecs_map_free(map);
    PASS();
}

SUITE(map)
{
    RUN_TEST(map_empty);
    RUN_TEST(map_set);
    RUN_TEST(map_get);
    RUN_TEST(map_set_multiple);
    RUN_TEST(map_update);
}

TEST ecs_minimal()
{
    ecs_world_t *world = ecs_init();
    ecs_destroy(world);
    PASS();
}

SUITE(ecs)
{
    RUN_TEST(ecs_minimal);
}

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[])
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(map);
    RUN_SUITE(ecs);
    GREATEST_MAIN_END();
}
