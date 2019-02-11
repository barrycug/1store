/*
 * Copyright (C) 2016 ScyllaDB
 *
 * Modified by ScyllaDB
 */

/*
 * This file is part of Scylla.
 *
 * Scylla is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Scylla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Scylla.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/range/irange.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/test/unit_test.hpp>
#include <stdint.h>

#include "tests/test-utils.hh"
#include "tests/cql_test_env.hh"
#include "tests/cql_assertions.hh"
#include "tests/test_services.hh"

#include "index/secondary_index_manager.hh"
#include "db/size_estimates_virtual_reader.hh"
#include "db/system_keyspace.hh"
#include "db/view/view_builder.hh"
#include "core/future-util.hh"
#include "cql3/query_processor.hh"
#include "cql3/untyped_result_set.hh"
#include "dht/i_partitioner.hh"
#include "transport/messages/result_message.hh"

using namespace std::literals::chrono_literals;

SEASTAR_TEST_CASE(test_query_size_estimates_virtual_table) {
    return do_with_cql_env([] (auto& e) {
        auto ranges = db::size_estimates::size_estimates_mutation_reader::get_local_ranges().get0();
        auto start_token1 = utf8_type->to_string(ranges[3].start);
        auto start_token2 = utf8_type->to_string(ranges[5].start);
        auto end_token1 = utf8_type->to_string(ranges[3].end);
        auto end_token2 = utf8_type->to_string(ranges[55].end);
        auto &qp = e.local_qp();
        return e.execute_cql("create table cf1(pk text PRIMARY KEY, v int);").discard_result().then([&e] {
            return e.execute_cql("create table cf2(pk text PRIMARY KEY, v int);").discard_result();
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks';").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 512);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' limit 100;").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 100);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' "
                                       "and table_name = 'cf1';").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 256);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' "
                                       "and table_name > 'cf1';").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 256);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' "
                                       "and table_name >= 'cf1';").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 512);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' "
                                       "and table_name < 'cf2';").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 256);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' "
                                       "and table_name <= 'cf2';").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 512);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' "
                                       "and table_name in ('cf1', 'cf2');").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 512);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' "
                                       "and table_name >= 'cf1' and table_name <= 'cf1';").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 256);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' "
                                       "and table_name >= 'cf1' and table_name <= 'cf2';").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 512);
            });
        }).then([&qp] {
            return qp.execute_internal("select * from system.size_estimates where keyspace_name = 'ks' "
                                       "and table_name > 'cf1' and table_name < 'cf2';").then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 0);
            });
        }).then([&qp, start_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start = '%s';", start_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 1);
            });
        }).then([&qp, start_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start >= '%s';", start_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 253);
            });
        }).then([&qp, start_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start > '%s';", start_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 252);
            });
        }).then([&qp, start_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start <= '%s';", start_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 4);
            });
         }).then([&qp, start_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start < '%s';", start_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 3);
            });
          }).then([&qp, start_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start >= '%s' and range_start <= '%s';", start_token1, start_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 1);
            });
        }).then([&qp, start_token1, start_token2] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start >= '%s' and range_start <= '%s';", start_token1, start_token2)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 3);
            });
        }).then([&qp, start_token1, start_token2] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start > '%s' and range_start < '%s';", start_token1, start_token2)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 1);
            });
        }).then([&qp, start_token1, start_token2] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start in ('%s', '%s');", start_token1, start_token2)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 2);
            });
        }).then([&qp, start_token1, start_token2] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start > '%s' and range_start <= '%s';", start_token1, start_token2)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 2);
            });
        }).then([&qp, start_token1, start_token2] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start >= '%s' and range_start < '%s';", start_token1, start_token2)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 2);
            });
        }).then([&qp, start_token1, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start = '%s' and range_end = '%s';", start_token1, end_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 1);
            });
        }).then([&qp, start_token1, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start = '%s' and range_end >= '%s';", start_token1, end_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 1);
            });
        }).then([&qp, start_token1, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start = '%s' and range_end > '%s';", start_token1, end_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 0);
            });
        }).then([&qp, start_token1, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start = '%s' and range_end <= '%s';", start_token1, end_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 1);
            });
        }).then([&qp, start_token1, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start = '%s' and range_end < '%s';", start_token1, end_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 0);
            });
        }).then([&qp, start_token1, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start = '%s' and range_end >= '%s' and range_end <= '%s';", start_token1, end_token1, end_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 1);
            });
        }).then([&qp, start_token1, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and table_name = 'cf1' and range_start = '%s' and range_end > '%s' and range_end < '%s';", start_token1, end_token1, end_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 0);
            });
        }).then([&qp, start_token1, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and (table_name, range_start, range_end) = ('cf1', '%s', '%s');", start_token1, end_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 1);
            });
        }).then([&qp, start_token1, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and (table_name, range_start, range_end) >= ('cf1', '%s', '%s') and (table_name) <= ('cf2');", start_token1, end_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 509);
            });
        }).then([&qp, start_token1, start_token2, end_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and (table_name, range_start, range_end) >= ('cf1', '%s', '%s') "
                                              "and (table_name, range_start) <= ('cf2', '%s');", start_token1, end_token1, start_token2)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 259);
            });
        }).then([&qp, start_token1] {
            return qp.execute_internal(sprint("select * from system.size_estimates where keyspace_name = 'ks' "
                                              "and (table_name, range_start) < ('cf2', '%s');", start_token1)).then([](auto rs) {
                BOOST_REQUIRE_EQUAL(rs->size(), 259);
            });
        }).discard_result();
    });
}

SEASTAR_TEST_CASE(test_query_view_built_progress_virtual_table) {
    return do_with_cql_env_thread([] (cql_test_env& e) {
        auto rand = [] { return dht::global_partitioner().get_random_token(); };
        auto next_token = rand();
        auto next_token_str = dht::global_partitioner().to_sstring(next_token);
        db::system_keyspace::register_view_for_building("ks", "v1", rand()).get();
        db::system_keyspace::register_view_for_building("ks", "v2", rand()).get();
        db::system_keyspace::register_view_for_building("ks", "v3", rand()).get();
        db::system_keyspace::update_view_build_progress("ks", "v3", next_token).get();
        db::system_keyspace::register_view_for_building("ks", "v4", rand()).get();
        db::system_keyspace::update_view_build_progress("ks", "v4", next_token).get();
        db::system_keyspace::register_view_for_building("ks", "v5", rand()).get();
        db::system_keyspace::register_view_for_building("ks", "v6", rand()).get();
        db::system_keyspace::remove_view_build_progress_across_all_shards("ks", "v5").get();
        db::system_keyspace::remove_view_build_progress_across_all_shards("ks", "v6").get();
        auto rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks'").get0();
        assert_that(rs).is_rows().with_rows_ignore_order({
                { {utf8_type->decompose(sstring("ks"))}, {utf8_type->decompose(sstring("v1"))}, {int32_type->decompose(0)}, { } },
                { {utf8_type->decompose(sstring("ks"))}, {utf8_type->decompose(sstring("v2"))}, {int32_type->decompose(0)}, { } },
                { {utf8_type->decompose(sstring("ks"))}, {utf8_type->decompose(sstring("v3"))}, {int32_type->decompose(0)}, {utf8_type->decompose(next_token_str)} },
                { {utf8_type->decompose(sstring("ks"))}, {utf8_type->decompose(sstring("v4"))}, {int32_type->decompose(0)}, {utf8_type->decompose(next_token_str)} }
        });
        rs = e.execute_cql("select * from system.views_builds_in_progress").get0();
        assert_that(rs).is_rows().with_size(4);
        rs = e.execute_cql("select * from system.views_builds_in_progress limit 1").get0();
        assert_that(rs).is_rows().with_size(1);
        rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks' and view_name = 'v2'").get0();
        assert_that(rs).is_rows().with_size(1);
        rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks' and view_name > 'v2'").get0();
        assert_that(rs).is_rows().with_size(2);
        rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks' and view_name >= 'v2'").get0();
        assert_that(rs).is_rows().with_size(3);
        rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks' and view_name  < 'v2'").get0();
        assert_that(rs).is_rows().with_size(1);
        rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks' and view_name  <= 'v2'").get0();
        assert_that(rs).is_rows().with_size(2);
        rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks' and view_name in ('v1', 'v2', 'v3')").get0();
        assert_that(rs).is_rows().with_size(3);
        rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks' and view_name >= 'v2' and view_name  <= 'v2'").get0();
        assert_that(rs).is_rows().with_size(1);
        rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks' and view_name >= 'v2' and view_name  <= 'v3'").get0();
        assert_that(rs).is_rows().with_size(2);
        rs = e.execute_cql("select * from system.views_builds_in_progress where keyspace_name = 'ks' and view_name > 'v1' and view_name  < 'v2'").get0();
        assert_that(rs).is_rows().with_size(0);
    });
}

SEASTAR_TEST_CASE(test_query_built_indexes_virtual_table) {
    return do_with_cql_env_thread([] (cql_test_env& e) {
        auto idx = secondary_index::index_table_name("idx");
        e.execute_cql("create table cf(p int PRIMARY KEY, v int);").get();
        auto f1 = e.local_view_builder().wait_until_built("ks", "vcf");
        auto f2 = e.local_view_builder().wait_until_built("ks", idx);
        e.execute_cql("create materialized view vcf as select * from cf "
                      "where v is not null and p is not null "
                      "primary key (v, p)").get();
        e.execute_cql("create index idx on cf (v)").get();
        f1.get();
        f2.get();
        auto rs = e.execute_cql("select * from system.built_views").get0();
        assert_that(rs).is_rows().with_rows_ignore_order({
                { {utf8_type->decompose(sstring("ks"))}, {utf8_type->decompose(idx)} },
                { {utf8_type->decompose(sstring("ks"))}, {"vcf"} },
        });
        rs = e.execute_cql("select * from system.\"IndexInfo\"").get0();
        assert_that(rs).is_rows().with_rows_ignore_order({
                { {utf8_type->decompose(sstring("ks"))}, {utf8_type->decompose(idx)} },
        });
        rs = e.execute_cql(sprint("select * from system.\"IndexInfo\" where table_name = 'ks' and index_name = '%s'", idx)).get0();
        assert_that(rs).is_rows().with_size(1);
        rs = e.execute_cql("select * from system.\"IndexInfo\" where table_name = 'ks' and index_name = 'vcf'").get0();
        assert_that(rs).is_rows().with_size(0);
    });
}