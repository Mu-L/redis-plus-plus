/**************************************************************************
   Copyright (c) 2017 sewenew

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *************************************************************************/

#ifndef SEWENEW_REDISPLUSPLUS_REDIS_CLUSTER_HPP
#define SEWENEW_REDISPLUSPLUS_REDIS_CLUSTER_HPP

#include <utility>
#include "sw/redis++/command.h"
#include "sw/redis++/reply.h"
#include "sw/redis++/utils.h"
#include "sw/redis++/errors.h"
#include "sw/redis++/shards_pool.h"

namespace sw {

namespace redis {

template <typename Callback>
void RedisCluster::for_each(Callback &&cb) {
    // Update the underlying slot-node mapping to ensure we get the latest one.
    _pool->update();

    auto pools = _pool->pools();
    for (auto &pool : pools) {
        auto connection = std::make_shared<GuardedConnection>(pool);
        auto r = Redis(connection);

        cb(r);
    }
}

template <typename Cmd, typename Key, typename ...Args>
auto RedisCluster::command(Cmd cmd, Key &&key, Args &&...args)
    -> typename std::enable_if<!std::is_convertible<Cmd, StringView>::value, ReplyUPtr>::type {
    return _command(cmd,
                    std::is_convertible<typename std::decay<Key>::type, StringView>(),
                    std::forward<Key>(key),
                    std::forward<Args>(args)...);
}

template <typename Key, typename ...Args>
auto RedisCluster::command(const StringView &cmd_name, Key &&key, Args &&...args)
    -> typename std::enable_if<(std::is_convertible<Key, StringView>::value
        || std::is_arithmetic<typename std::decay<Key>::type>::value)
        && !IsIter<typename LastType<Key, Args...>::type>::value, ReplyUPtr>::type {
    auto cmd = Command(cmd_name);

    return _generic_command(cmd, std::forward<Key>(key), std::forward<Args>(args)...);
}

template <typename Result, typename Key, typename ...Args>
auto RedisCluster::command(const StringView &cmd_name, Key &&key, Args &&...args)
    -> typename std::enable_if<std::is_convertible<Key, StringView>::value
            || std::is_arithmetic<typename std::decay<Key>::type>::value, Result>::type {
    auto r = command(cmd_name, std::forward<Key>(key), std::forward<Args>(args)...);

    assert(r);

    return reply::parse<Result>(*r);
}

template <typename Key, typename ...Args>
auto RedisCluster::command(const StringView &cmd_name, Key &&key, Args &&...args)
    -> typename std::enable_if<(std::is_convertible<Key, StringView>::value
            || std::is_arithmetic<typename std::decay<Key>::type>::value)
            && IsIter<typename LastType<Key, Args...>::type>::value, void>::type {
    auto r = _command(cmd_name,
                        MakeIndexSequence<sizeof...(Args)>(),
                        std::forward<Key>(key),
                        std::forward<Args>(args)...);

    assert(r);

    reply::to_array(*r, LastValue(std::forward<Args>(args)...));
}

template <typename Input>
auto RedisCluster::command(Input first, Input last)
    -> typename std::enable_if<IsIter<Input>::value, ReplyUPtr>::type {
    if (first == last || std::next(first) == last) {
        throw Error("command: invalid range");
    }

    const auto &key = *first;
    ++first;

    auto cmd = [&key](Connection &connection, Input start, Input stop) {
                        CmdArgs cmd_args;
                        cmd_args.append(key);
                        while (start != stop) {
                            cmd_args.append(*start);
                            ++start;
                        }
                        connection.send(cmd_args);
    };

    return command(cmd, first, last);
}

template <typename Result, typename Input>
auto RedisCluster::command(Input first, Input last)
    -> typename std::enable_if<IsIter<Input>::value, Result>::type {
    auto r = command(first, last);

    assert(r);

    return reply::parse<Result>(*r);
}

template <typename Input, typename Output>
auto RedisCluster::command(Input first, Input last, Output output)
    -> typename std::enable_if<IsIter<Input>::value, void>::type {
    auto r = command(first, last);

    assert(r);

    reply::to_array(*r, output);
}

// KEY commands.

template <typename Input>
long long RedisCluster::del(Input first, Input last) {
    range_check("DEL", first, last);

    auto reply = command(cmd::del_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input>
long long RedisCluster::exists(Input first, Input last) {
    range_check("EXISTS", first, last);

    auto reply = command(cmd::exists_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

inline bool RedisCluster::expire(const StringView &key, const std::chrono::seconds &timeout) {
    return expire(key, timeout.count());
}

inline bool RedisCluster::expireat(const StringView &key,
                                    const std::chrono::time_point<std::chrono::system_clock,
                                                                    std::chrono::seconds> &tp) {
    return expireat(key, tp.time_since_epoch().count());
}

inline bool RedisCluster::pexpire(const StringView &key, const std::chrono::milliseconds &timeout) {
    return pexpire(key, timeout.count());
}

inline bool RedisCluster::pexpireat(const StringView &key,
                                const std::chrono::time_point<std::chrono::system_clock,
                                                                std::chrono::milliseconds> &tp) {
    return pexpireat(key, tp.time_since_epoch().count());
}

inline void RedisCluster::restore(const StringView &key,
                            const StringView &val,
                            const std::chrono::milliseconds &ttl,
                            bool replace) {
    return restore(key, val, ttl.count(), replace);
}

template <typename Input>
long long RedisCluster::touch(Input first, Input last) {
    range_check("TOUCH", first, last);

    auto reply = command(cmd::touch_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input>
long long RedisCluster::unlink(Input first, Input last) {
    range_check("UNLINK", first, last);

    auto reply = command(cmd::unlink_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

// STRING commands.

template <typename Input>
long long RedisCluster::bitop(BitOp op, const StringView &destination, Input first, Input last) {
    range_check("BITOP", first, last);

    auto reply = _command(cmd::bitop_range<Input>, destination, op, destination, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input, typename Output>
void RedisCluster::mget(Input first, Input last, Output output) {
    range_check("MGET", first, last);

    auto reply = command(cmd::mget<Input>, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
void RedisCluster::mset(Input first, Input last) {
    range_check("MSET", first, last);

    auto reply = command(cmd::mset<Input>, first, last);

    reply::parse<void>(*reply);
}

template <typename Input>
bool RedisCluster::msetnx(Input first, Input last) {
    range_check("MSETNX", first, last);

    auto reply = command(cmd::msetnx<Input>, first, last);

    return reply::parse<bool>(*reply);
}

inline void RedisCluster::psetex(const StringView &key,
                            const std::chrono::milliseconds &ttl,
                            const StringView &val) {
    return psetex(key, ttl.count(), val);
}

inline void RedisCluster::setex(const StringView &key,
                            const std::chrono::seconds &ttl,
                            const StringView &val) {
    setex(key, ttl.count(), val);
}

// LIST commands.

template <typename Input>
OptionalStringPair RedisCluster::blpop(Input first, Input last, long long timeout) {
    range_check("BLPOP", first, last);

    auto reply = command(cmd::blpop_range<Input>, first, last, timeout);

    return reply::parse<OptionalStringPair>(*reply);
}

template <typename Input>
OptionalStringPair RedisCluster::blpop(Input first,
                                Input last,
                                const std::chrono::seconds &timeout) {
    return blpop(first, last, timeout.count());
}

template <typename Input>
OptionalStringPair RedisCluster::brpop(Input first, Input last, long long timeout) {
    range_check("BRPOP", first, last);

    auto reply = command(cmd::brpop_range<Input>, first, last, timeout);

    return reply::parse<OptionalStringPair>(*reply);
}

template <typename Input>
OptionalStringPair RedisCluster::brpop(Input first,
                                Input last,
                                const std::chrono::seconds &timeout) {
    return brpop(first, last, timeout.count());
}

inline OptionalString RedisCluster::brpoplpush(const StringView &source,
                                        const StringView &destination,
                                        const std::chrono::seconds &timeout) {
    return brpoplpush(source, destination, timeout.count());
}

template <typename Input>
inline long long RedisCluster::lpush(const StringView &key, Input first, Input last) {
    range_check("LPUSH", first, last);

    auto reply = command(cmd::lpush_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
inline void RedisCluster::lrange(const StringView &key, long long start, long long stop, Output output) {
    auto reply = command(cmd::lrange, key, start, stop);

    reply::to_array(*reply, output);
}

template <typename Input>
inline long long RedisCluster::rpush(const StringView &key, Input first, Input last) {
    range_check("RPUSH", first, last);

    auto reply = command(cmd::rpush_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output, typename Input>
Optional<std::pair<std::string, Output>> RedisCluster::lmpop(Input first, Input last, ListWhence whence, long long count) {
    range_check("LMPOP", first, last);

    auto reply = command(cmd::lmpop<Input>, first, last, whence, count);

    return reply::parse<Optional<std::pair<std::string, Output>>>(*reply);
}

// HASH commands.

template <typename Input>
inline long long RedisCluster::hdel(const StringView &key, Input first, Input last) {
    range_check("HDEL", first, last);

    auto reply = command(cmd::hdel_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
inline void RedisCluster::hgetall(const StringView &key, Output output) {
    auto reply = command(cmd::hgetall, key);

    reply::to_array(*reply, output);
}

template <typename Output>
inline void RedisCluster::hkeys(const StringView &key, Output output) {
    auto reply = command(cmd::hkeys, key);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
inline void RedisCluster::hmget(const StringView &key, Input first, Input last, Output output) {
    range_check("HMGET", first, last);

    auto reply = command(cmd::hmget<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
inline void RedisCluster::hmset(const StringView &key, Input first, Input last) {
    range_check("HMSET", first, last);

    auto reply = command(cmd::hmset<Input>, key, first, last);

    reply::parse<void>(*reply);
}

template <typename Output>
Cursor RedisCluster::hscan(const StringView &key,
                     Cursor cursor,
                     const StringView &pattern,
                     long long count,
                     Output output) {
    auto reply = command(cmd::hscan, key, cursor, pattern, count);

    return reply::parse_scan_reply(*reply, output);
}

template <typename Output>
inline Cursor RedisCluster::hscan(const StringView &key,
                             Cursor cursor,
                             const StringView &pattern,
                             Output output) {
    return hscan(key, cursor, pattern, 10, output);
}

template <typename Output>
inline Cursor RedisCluster::hscan(const StringView &key,
                             Cursor cursor,
                             long long count,
                             Output output) {
    return hscan(key, cursor, "*", count, output);
}

template <typename Output>
inline Cursor RedisCluster::hscan(const StringView &key,
                             Cursor cursor,
                             Output output) {
    return hscan(key, cursor, "*", 10, output);
}

template <typename Input>
auto RedisCluster::hset(const StringView &key, Input first, Input last)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    long long>::type {
    range_check("HSET", first, last);

    auto reply = command(cmd::hset_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input>
auto RedisCluster::hsetex(const StringView &key,
        Input first,
        Input last,
        bool keep_ttl,
        HSetExOption opt)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    long long>::type {
    range_check("HSETEX", first, last);

    auto reply = command(cmd::hsetex_keep_ttl_range<Input>, key, first, last, keep_ttl, opt);

    return reply::parse<long long>(*reply);
}

template <typename Input>
auto RedisCluster::hsetex(const StringView &key,
        Input first,
        Input last,
        const std::chrono::milliseconds &ttl,
        HSetExOption opt)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    long long>::type {
    range_check("HSETEX", first, last);

    auto reply = command(cmd::hsetex_ttl_range<Input>, key, first, last, ttl, opt);

    return reply::parse<long long>(*reply);
}

template <typename Input>
auto RedisCluster::hsetex(const StringView &key,
        Input first,
        Input last,
        const std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> &tp,
        HSetExOption opt)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                    long long>::type {
    range_check("HSETEX", first, last);

    auto reply = command(cmd::hsetex_time_point_range<Input>, key, first, last, tp, opt);

    return reply::parse<long long>(*reply);
}

template <typename Input, typename Output>
void RedisCluster::httl(const StringView &key, Input first, Input last, Output output) {
    range_check("HTTL", first, last);

    auto reply = command(cmd::httl_range<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
void RedisCluster::hpttl(const StringView &key, Input first, Input last, Output output) {
    range_check("HPTTL", first, last);

    auto reply = command(cmd::hpttl_range<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
void RedisCluster::hexpiretime(const StringView &key, Input first, Input last, Output output) {
    range_check("HEXPIRETIME", first, last);

    auto reply = command(cmd::hexpiretime_range<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
void RedisCluster::hpexpiretime(const StringView &key, Input first, Input last, Output output) {
    range_check("HPEXPIRETIME", first, last);

    auto reply = command(cmd::hpexpiretime_range<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
void RedisCluster::hpexpire(const StringView &key,
        Input first,
        Input last,
        const std::chrono::milliseconds &ttl,
        Output output) {
    range_check("HPEXPIRE", first, last);

    auto reply = command(cmd::hpexpire_range<Input>, key, first, last, ttl, HPExpireOption::ALWAYS);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
void RedisCluster::hpexpire(const StringView &key,
        Input first,
        Input last,
        const std::chrono::milliseconds &ttl,
        HPExpireOption opt,
        Output output) {
    range_check("HPEXPIRE", first, last);

    auto reply = command(cmd::hpexpire_range<Input>, key, first, last, ttl, opt);

    reply::to_array(*reply, output);
}

template <typename Output>
inline void RedisCluster::hvals(const StringView &key, Output output) {
    auto reply = command(cmd::hvals, key);

    reply::to_array(*reply, output);
}

// SET commands.

template <typename Input>
long long RedisCluster::sadd(const StringView &key, Input first, Input last) {
    range_check("SADD", first, last);

    auto reply = command(cmd::sadd_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input, typename Output>
void RedisCluster::sdiff(Input first, Input last, Output output) {
    range_check("SDIFF", first, last);

    auto reply = command(cmd::sdiff<Input>, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::sdiffstore(const StringView &destination,
                                    Input first,
                                    Input last) {
    range_check("SDIFFSTORE", first, last);

    auto reply = command(cmd::sdiffstore_range<Input>, destination, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input, typename Output>
void RedisCluster::sinter(Input first, Input last, Output output) {
    range_check("SINTER", first, last);

    auto reply = command(cmd::sinter<Input>, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::sinterstore(const StringView &destination,
                                    Input first,
                                    Input last) {
    range_check("SINTERSTORE", first, last);

    auto reply = command(cmd::sinterstore_range<Input>, destination, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
void RedisCluster::smembers(const StringView &key, Output output) {
    auto reply = command(cmd::smembers, key);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::spop(const StringView &key, long long count, Output output) {
    auto reply = command(cmd::spop_range, key, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::srandmember(const StringView &key, long long count, Output output) {
    auto reply = command(cmd::srandmember_range, key, count);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::srem(const StringView &key, Input first, Input last) {
    range_check("SREM", first, last);

    auto reply = command(cmd::srem_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
Cursor RedisCluster::sscan(const StringView &key,
                     Cursor cursor,
                     const StringView &pattern,
                     long long count,
                     Output output) {
    auto reply = command(cmd::sscan, key, cursor, pattern, count);

    return reply::parse_scan_reply(*reply, output);
}

template <typename Output>
inline Cursor RedisCluster::sscan(const StringView &key,
                             Cursor cursor,
                             const StringView &pattern,
                             Output output) {
    return sscan(key, cursor, pattern, 10, output);
}

template <typename Output>
inline Cursor RedisCluster::sscan(const StringView &key,
                             Cursor cursor,
                             long long count,
                             Output output) {
    return sscan(key, cursor, "*", count, output);
}

template <typename Output>
inline Cursor RedisCluster::sscan(const StringView &key,
                             Cursor cursor,
                             Output output) {
    return sscan(key, cursor, "*", 10, output);
}

template <typename Input, typename Output>
void RedisCluster::sunion(Input first, Input last, Output output) {
    range_check("SUNION", first, last);

    auto reply = command(cmd::sunion<Input>, first, last);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::sunionstore(const StringView &destination, Input first, Input last) {
    range_check("SUNIONSTORE", first, last);

    auto reply = command(cmd::sunionstore_range<Input>, destination, first, last);

    return reply::parse<long long>(*reply);
}

// SORTED SET commands.

inline auto RedisCluster::bzpopmax(const StringView &key, const std::chrono::seconds &timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    return bzpopmax(key, timeout.count());
}

template <typename Input>
auto RedisCluster::bzpopmax(Input first, Input last, long long timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    auto reply = command(cmd::bzpopmax_range<Input>, first, last, timeout);

    return reply::parse<Optional<std::tuple<std::string, std::string, double>>>(*reply);
}

template <typename Input>
inline auto RedisCluster::bzpopmax(Input first,
                                    Input last,
                                    const std::chrono::seconds &timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    return bzpopmax(first, last, timeout.count());
}

inline auto RedisCluster::bzpopmin(const StringView &key, const std::chrono::seconds &timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    return bzpopmin(key, timeout.count());
}

template <typename Input>
auto RedisCluster::bzpopmin(Input first, Input last, long long timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    auto reply = command(cmd::bzpopmin_range<Input>, first, last, timeout);

    return reply::parse<Optional<std::tuple<std::string, std::string, double>>>(*reply);
}

template <typename Input>
inline auto RedisCluster::bzpopmin(Input first,
                                    Input last,
                                    const std::chrono::seconds &timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    return bzpopmin(first, last, timeout.count());
}

template <typename Input>
long long RedisCluster::zadd(const StringView &key,
                        Input first,
                        Input last,
                        UpdateType type,
                        bool changed) {
    range_check("ZADD", first, last);

    auto reply = command(cmd::zadd_range<Input>, key, first, last, type, changed);

    return reply::parse<long long>(*reply);
}

template <typename Interval>
long long RedisCluster::zcount(const StringView &key, const Interval &interval) {
    auto reply = command(cmd::zcount<Interval>, key, interval);

    return reply::parse<long long>(*reply);
}

template <typename Input>
long long RedisCluster::zinterstore(const StringView &destination,
                                Input first,
                                Input last,
                                Aggregation type) {
    range_check("ZINTERSTORE", first, last);

    auto reply = command(cmd::zinterstore_range<Input>,
                            destination,
                            first,
                            last,
                            type);

    return reply::parse<long long>(*reply);
}

template <typename Interval>
long long RedisCluster::zlexcount(const StringView &key, const Interval &interval) {
    auto reply = command(cmd::zlexcount<Interval>, key, interval);

    return reply::parse<long long>(*reply);
}

template <typename Output>
void RedisCluster::zpopmax(const StringView &key, long long count, Output output) {
    auto reply = command(cmd::zpopmax, key, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::zpopmin(const StringView &key, long long count, Output output) {
    auto reply = command(cmd::zpopmin, key, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::zrange(const StringView &key, long long start, long long stop, Output output) {
    auto reply = _score_command<Output>(cmd::zrange, key, start, stop);

    reply::to_array(*reply, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrangebylex(const StringView &key, const Interval &interval, Output output) {
    zrangebylex(key, interval, {}, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrangebylex(const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts,
                        Output output) {
    auto reply = command(cmd::zrangebylex<Interval>, key, interval, opts);

    reply::to_array(*reply, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrangebyscore(const StringView &key,
                            const Interval &interval,
                            Output output) {
    zrangebyscore(key, interval, {}, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrangebyscore(const StringView &key,
                            const Interval &interval,
                            const LimitOptions &opts,
                            Output output) {
    auto reply = _score_command<Output>(cmd::zrangebyscore<Interval>,
                                        key,
                                        interval,
                                        opts);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::zrem(const StringView &key, Input first, Input last) {
    range_check("ZREM", first, last);

    auto reply = command(cmd::zrem_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Interval>
long long RedisCluster::zremrangebylex(const StringView &key, const Interval &interval) {
    auto reply = command(cmd::zremrangebylex<Interval>, key, interval);

    return reply::parse<long long>(*reply);
}

template <typename Interval>
long long RedisCluster::zremrangebyscore(const StringView &key, const Interval &interval) {
    auto reply = command(cmd::zremrangebyscore<Interval>, key, interval);

    return reply::parse<long long>(*reply);
}

template <typename Output>
void RedisCluster::zrevrange(const StringView &key, long long start, long long stop, Output output) {
    auto reply = _score_command<Output>(cmd::zrevrange, key, start, stop);

    reply::to_array(*reply, output);
}

template <typename Interval, typename Output>
inline void RedisCluster::zrevrangebylex(const StringView &key,
                                    const Interval &interval,
                                    Output output) {
    zrevrangebylex(key, interval, {}, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrevrangebylex(const StringView &key,
                            const Interval &interval,
                            const LimitOptions &opts,
                            Output output) {
    auto reply = command(cmd::zrevrangebylex<Interval>, key, interval, opts);

    reply::to_array(*reply, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrevrangebyscore(const StringView &key, const Interval &interval, Output output) {
    zrevrangebyscore(key, interval, {}, output);
}

template <typename Interval, typename Output>
void RedisCluster::zrevrangebyscore(const StringView &key,
                                const Interval &interval,
                                const LimitOptions &opts,
                                Output output) {
    auto reply = _score_command<Output>(cmd::zrevrangebyscore<Interval>, key, interval, opts);

    reply::to_array(*reply, output);
}

template <typename Output>
Cursor RedisCluster::zscan(const StringView &key,
                     Cursor cursor,
                     const StringView &pattern,
                     long long count,
                     Output output) {
    auto reply = command(cmd::zscan, key, cursor, pattern, count);

    return reply::parse_scan_reply(*reply, output);
}

template <typename Output>
inline Cursor RedisCluster::zscan(const StringView &key,
                             Cursor cursor,
                             const StringView &pattern,
                             Output output) {
    return zscan(key, cursor, pattern, 10, output);
}

template <typename Output>
inline Cursor RedisCluster::zscan(const StringView &key,
                             Cursor cursor,
                             long long count,
                             Output output) {
    return zscan(key, cursor, "*", count, output);
}

template <typename Output>
inline Cursor RedisCluster::zscan(const StringView &key,
                             Cursor cursor,
                             Output output) {
    return zscan(key, cursor, "*", 10, output);
}

template <typename Input>
long long RedisCluster::zunionstore(const StringView &destination,
                                    Input first,
                                    Input last,
                                    Aggregation type) {
    range_check("ZUNIONSTORE", first, last);

    auto reply = command(cmd::zunionstore_range<Input>,
                            destination,
                            first,
                            last,
                            type);

    return reply::parse<long long>(*reply);
}

// HYPERLOGLOG commands.

template <typename Input>
bool RedisCluster::pfadd(const StringView &key, Input first, Input last) {
    range_check("PFADD", first, last);

    auto reply = command(cmd::pfadd_range<Input>, key, first, last);

    return reply::parse<bool>(*reply);
}

template <typename Input>
long long RedisCluster::pfcount(Input first, Input last) {
    range_check("PFCOUNT", first, last);

    auto reply = command(cmd::pfcount_range<Input>, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input>
void RedisCluster::pfmerge(const StringView &destination,
                    Input first,
                    Input last) {
    range_check("PFMERGE", first, last);

    auto reply = command(cmd::pfmerge_range<Input>, destination, first, last);

    reply::parse<void>(*reply);
}

// GEO commands.

template <typename Input>
inline long long RedisCluster::geoadd(const StringView &key,
                                Input first,
                                Input last) {
    range_check("GEOADD", first, last);

    auto reply = command(cmd::geoadd_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input, typename Output>
void RedisCluster::geohash(const StringView &key, Input first, Input last, Output output) {
    range_check("GEOHASH", first, last);

    auto reply = command(cmd::geohash_range<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
void RedisCluster::geopos(const StringView &key, Input first, Input last, Output output) {
    range_check("GEOPOS", first, last);

    auto reply = command(cmd::geopos_range<Input>, key, first, last);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::georadius(const StringView &key,
                        const std::pair<double, double> &loc,
                        double radius,
                        GeoUnit unit,
                        long long count,
                        bool asc,
                        Output output) {
    auto reply = command(cmd::georadius,
                            key,
                            loc,
                            radius,
                            unit,
                            count,
                            asc,
                            WithCoord<typename IterType<Output>::type>::value,
                            WithDist<typename IterType<Output>::type>::value,
                            WithHash<typename IterType<Output>::type>::value);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::georadiusbymember(const StringView &key,
                                const StringView &member,
                                double radius,
                                GeoUnit unit,
                                long long count,
                                bool asc,
                                Output output) {
    auto reply = command(cmd::georadiusbymember,
                            key,
                            member,
                            radius,
                            unit,
                            count,
                            asc,
                            WithCoord<typename IterType<Output>::type>::value,
                            WithDist<typename IterType<Output>::type>::value,
                            WithHash<typename IterType<Output>::type>::value);

    reply::to_array(*reply, output);
}

// SCRIPTING commands.

template <typename Result, typename Keys, typename Args>
Result RedisCluster::eval(const StringView &script,
                          Keys keys_first,
                          Keys keys_last,
                          Args args_first,
                          Args args_last) {
    if (keys_first == keys_last) {
        throw Error("DO NOT support Lua script without key");
    }

    auto reply = _command(cmd::eval<Keys, Args>, *keys_first, script, keys_first, keys_last, args_first, args_last);

    return reply::parse<Result>(*reply);
}

template <typename Result>
Result RedisCluster::eval(const StringView &script,
                            std::initializer_list<StringView> keys,
                            std::initializer_list<StringView> args) {
    return eval<Result>(script, keys.begin(), keys.end(), args.begin(), args.end());
}

template <typename Keys, typename Args, typename Output>
void RedisCluster::eval(const StringView &script,
                          Keys keys_first,
                          Keys keys_last,
                          Args args_first,
                          Args args_last,
                          Output output) {
    if (keys_first == keys_last) {
        throw Error("DO NOT support Lua script without key");
    }

    auto reply = _command(cmd::eval<Keys, Args>,
                            *keys_first,
                            script,
                            keys_first, keys_last,
                            args_first, args_last);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::eval(const StringView &script,
                        std::initializer_list<StringView> keys,
                        std::initializer_list<StringView> args,
                        Output output) {
    eval(script, keys.begin(), keys.end(), args.begin(), args.end(), output);
}

template <typename Result, typename Keys, typename Args>
Result RedisCluster::evalsha(const StringView &script,
                              Keys keys_first,
                              Keys keys_last,
                              Args args_first,
                              Args args_last) {
    if (keys_first == keys_last) {
        throw Error("DO NOT support Lua script without key");
    }

    auto reply = _command(cmd::evalsha<Keys, Args>, *keys_first, script,
            keys_first, keys_last, args_first, args_last);

    return reply::parse<Result>(*reply);
}

template <typename Result>
Result RedisCluster::evalsha(const StringView &script,
                                std::initializer_list<StringView> keys,
                                std::initializer_list<StringView> args) {
    return evalsha<Result>(script, keys.begin(), keys.end(), args.begin(), args.end());
}

template <typename Keys, typename Args, typename Output>
void RedisCluster::evalsha(const StringView &script,
                              Keys keys_first,
                              Keys keys_last,
                              Args args_first,
                              Args args_last,
                              Output output) {
    if (keys_first == keys_last) {
        throw Error("DO NOT support Lua script without key");
    }

    auto reply = _command(cmd::evalsha<Keys, Args>,
                            *keys_first,
                            script,
                            keys_first, keys_last,
                            args_first, args_last);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::evalsha(const StringView &script,
                            std::initializer_list<StringView> keys,
                            std::initializer_list<StringView> args,
                            Output output) {
    evalsha(script, keys.begin(), keys.end(), args.begin(), args.end(), output);
}

template <typename Result, typename Keys, typename Args>
Result RedisCluster::fcall(const StringView &func,
                          Keys keys_first,
                          Keys keys_last,
                          Args args_first,
                          Args args_last) {
    if (keys_first == keys_last) {
        throw Error("DO NOT support function without key");
    }

    auto reply = _command(cmd::fcall<Keys, Args>, *keys_first, func, keys_first, keys_last, args_first, args_last);

    return reply::parse<Result>(*reply);
}

template <typename Result>
Result RedisCluster::fcall(const StringView &func,
                            std::initializer_list<StringView> keys,
                            std::initializer_list<StringView> args) {
    return fcall<Result>(func, keys.begin(), keys.end(), args.begin(), args.end());
}

template <typename Keys, typename Args, typename Output>
void RedisCluster::fcall(const StringView &func,
                          Keys keys_first,
                          Keys keys_last,
                          Args args_first,
                          Args args_last,
                          Output output) {
    if (keys_first == keys_last) {
        throw Error("DO NOT support function without key");
    }

    auto reply = _command(cmd::fcall<Keys, Args>,
                            *keys_first,
                            func,
                            keys_first, keys_last,
                            args_first, args_last);

    reply::to_array(*reply, output);
}

template <typename Result, typename Keys, typename Args>
Result RedisCluster::fcall_ro(const StringView &func,
                          Keys keys_first,
                          Keys keys_last,
                          Args args_first,
                          Args args_last) {
    if (keys_first == keys_last) {
        throw Error("DO NOT support function without key");
    }

    auto reply = _command(cmd::fcall_ro<Keys, Args>, *keys_first, func, keys_first, keys_last, args_first, args_last);

    return reply::parse<Result>(*reply);
}

template <typename Result>
Result RedisCluster::fcall_ro(const StringView &func,
                            std::initializer_list<StringView> keys,
                            std::initializer_list<StringView> args) {
    return fcall_ro<Result>(func, keys.begin(), keys.end(), args.begin(), args.end());
}

template <typename Keys, typename Args, typename Output>
void RedisCluster::fcall_ro(const StringView &func,
                          Keys keys_first,
                          Keys keys_last,
                          Args args_first,
                          Args args_last,
                          Output output) {
    if (keys_first == keys_last) {
        throw Error("DO NOT support function without key");
    }

    auto reply = _command(cmd::fcall_ro<Keys, Args>,
                            *keys_first,
                            func,
                            keys_first, keys_last,
                            args_first, args_last);

    reply::to_array(*reply, output);
}

// Stream commands.

template <typename Input>
long long RedisCluster::xack(const StringView &key,
                                const StringView &group,
                                Input first,
                                Input last) {
    auto reply = command(cmd::xack_range<Input>, key, group, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Input>
std::string RedisCluster::xadd(const StringView &key,
                                const StringView &id,
                                Input first,
                                Input last) {
    auto reply = command(cmd::xadd_range<Input>, key, id, first, last);

    return reply::parse<std::string>(*reply);
}

template <typename Input>
std::string RedisCluster::xadd(const StringView &key,
                                const StringView &id,
                                Input first,
                                Input last,
                                long long count,
                                bool approx) {
    auto reply = command(cmd::xadd_maxlen_range<Input>, key, id, first, last, count, approx);

    return reply::parse<std::string>(*reply);
}

template <typename Output>
void RedisCluster::xclaim(const StringView &key,
                            const StringView &group,
                            const StringView &consumer,
                            const std::chrono::milliseconds &min_idle_time,
                            const StringView &id,
                            Output output) {
    auto reply = command(cmd::xclaim, key, group, consumer, min_idle_time.count(), id);

    reply::to_array(*reply, output);
}

template <typename Input, typename Output>
void RedisCluster::xclaim(const StringView &key,
                            const StringView &group,
                            const StringView &consumer,
                            const std::chrono::milliseconds &min_idle_time,
                            Input first,
                            Input last,
                            Output output) {
    auto reply = command(cmd::xclaim_range<Input>,
                            key,
                            group,
                            consumer,
                            min_idle_time.count(),
                            first,
                            last);

    reply::to_array(*reply, output);
}

template <typename Input>
long long RedisCluster::xdel(const StringView &key, Input first, Input last) {
    auto reply = command(cmd::xdel_range<Input>, key, first, last);

    return reply::parse<long long>(*reply);
}

template <typename Output>
auto RedisCluster::xpending(const StringView &key, const StringView &group, Output output)
    -> std::tuple<long long, OptionalString, OptionalString> {
    auto reply = command(cmd::xpending, key, group);

    return reply::parse_xpending_reply(*reply, output);
}

template <typename Output>
void RedisCluster::xpending(const StringView &key,
                            const StringView &group,
                            const StringView &start,
                            const StringView &end,
                            long long count,
                            Output output) {
    auto reply = command(cmd::xpending_detail, key, group, start, end, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xpending(const StringView &key,
                            const StringView &group,
                            const StringView &start,
                            const StringView &end,
                            long long count,
                            const StringView &consumer,
                            Output output) {
    auto reply = command(cmd::xpending_per_consumer, key, group, start, end, count, consumer);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xrange(const StringView &key,
                            const StringView &start,
                            const StringView &end,
                            Output output) {
    auto reply = command(cmd::xrange, key, start, end);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xrange(const StringView &key,
                            const StringView &start,
                            const StringView &end,
                            long long count,
                            Output output) {
    auto reply = command(cmd::xrange_count, key, start, end, count);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xread(const StringView &key,
                            const StringView &id,
                            long long count,
                            Output output) {
    auto reply = command(cmd::xread, key, id, count);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Input, typename Output>
auto RedisCluster::xread(Input first, Input last, long long count, Output output)
    -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
    range_check("XREAD", first, last);

    auto reply = command(cmd::xread_range<Input>, first, last, count);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Output>
void RedisCluster::xread(const StringView &key,
                            const StringView &id,
                            const std::chrono::milliseconds &timeout,
                            long long count,
                            Output output) {
    auto reply = command(cmd::xread_block, key, id, timeout.count(), count);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Input, typename Output>
auto RedisCluster::xread(Input first,
                            Input last,
                            const std::chrono::milliseconds &timeout,
                            long long count,
                            Output output)
    -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
    range_check("XREAD", first, last);

    auto reply = command(cmd::xread_block_range<Input>, first, last, timeout.count(), count);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Output>
void RedisCluster::xreadgroup(const StringView &group,
                                const StringView &consumer,
                                const StringView &key,
                                const StringView &id,
                                long long count,
                                bool noack,
                                Output output) {
    auto reply = _command(cmd::xreadgroup, key, group, consumer, key, id, count, noack);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Input, typename Output>
auto RedisCluster::xreadgroup(const StringView &group,
                                const StringView &consumer,
                                Input first,
                                Input last,
                                long long count,
                                bool noack,
                                Output output)
    -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
    range_check("XREADGROUP", first, last);

    auto reply = _command(cmd::xreadgroup_range<Input>,
                            first->first,
                            group,
                            consumer,
                            first,
                            last,
                            count,
                            noack);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Output>
void RedisCluster::xreadgroup(const StringView &group,
                                const StringView &consumer,
                                const StringView &key,
                                const StringView &id,
                                const std::chrono::milliseconds &timeout,
                                long long count,
                                bool noack,
                                Output output) {
    auto reply = _command(cmd::xreadgroup_block,
                            key,
                            group,
                            consumer,
                            key,
                            id,
                            timeout.count(),
                            count,
                            noack);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Input, typename Output>
auto RedisCluster::xreadgroup(const StringView &group,
                                const StringView &consumer,
                                Input first,
                                Input last,
                                const std::chrono::milliseconds &timeout,
                                long long count,
                                bool noack,
                                Output output)
    -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
    range_check("XREADGROUP", first, last);

    auto reply = _command(cmd::xreadgroup_block_range<Input>,
                            first->first,
                            group,
                            consumer,
                            first,
                            last,
                            timeout.count(),
                            count,
                            noack);

    if (!reply::is_nil(*reply)) {
        reply::to_array(*reply, output);
    }
}

template <typename Output>
void RedisCluster::xrevrange(const StringView &key,
                            const StringView &end,
                            const StringView &start,
                            Output output) {
    auto reply = command(cmd::xrevrange, key, end, start);

    reply::to_array(*reply, output);
}

template <typename Output>
void RedisCluster::xrevrange(const StringView &key,
                                const StringView &end,
                                const StringView &start,
                                long long count,
                                Output output) {
    auto reply = command(cmd::xrevrange_count, key, end, start, count);

    reply::to_array(*reply, output);
}

template <typename Cmd, typename Key, typename ...Args>
auto RedisCluster::_generic_command(Cmd cmd, Key &&key, Args &&...args)
    -> typename std::enable_if<std::is_convertible<Key, StringView>::value,
                                ReplyUPtr>::type {
    return command(cmd, std::forward<Key>(key), std::forward<Args>(args)...);
}

template <typename Cmd, typename Key, typename ...Args>
auto RedisCluster::_generic_command(Cmd cmd, Key &&key, Args &&...args)
    -> typename std::enable_if<std::is_arithmetic<typename std::decay<Key>::type>::value,
                                ReplyUPtr>::type {
    auto k = std::to_string(std::forward<Key>(key));
    return command(cmd, k, std::forward<Args>(args)...);
}

template <typename Cmd, typename ...Args>
ReplyUPtr RedisCluster::_command(Cmd cmd, std::true_type, const StringView &key, Args &&...args) {
    return _command(cmd, key, key, std::forward<Args>(args)...);
}

template <typename Cmd, typename Input, typename ...Args>
ReplyUPtr RedisCluster::_command(Cmd cmd, std::false_type, Input &&first, Args &&...args) {
    return _range_command(cmd,
                            std::is_convertible<
                                typename std::decay<
                                    decltype(*std::declval<Input>())>::type, StringView>(),
                            std::forward<Input>(first),
                            std::forward<Args>(args)...);
}

template <typename Cmd, typename Input, typename ...Args>
ReplyUPtr RedisCluster::_range_command(Cmd cmd, std::true_type, Input input, Args &&...args) {
    return _command(cmd, *input, input, std::forward<Args>(args)...);
}

template <typename Cmd, typename Input, typename ...Args>
ReplyUPtr RedisCluster::_range_command(Cmd cmd, std::false_type, Input input, Args &&...args) {
    return _command(cmd, std::get<0>(*input), input, std::forward<Args>(args)...);
}

template <typename Cmd, typename ...Args>
ReplyUPtr RedisCluster::_command(Cmd cmd, Connection &connection, Args &&...args) {
    assert(!connection.broken());

    cmd(connection, std::forward<Args>(args)...);

    return connection.recv();
}

template <typename Cmd, typename ...Args>
ReplyUPtr RedisCluster::_command(Cmd cmd, const StringView &key, Args &&...args) {
    for (auto idx = 0; idx < 2; ++idx) {
        try {
            auto pool = _pool->fetch(key);
            assert(pool);
            SafeConnection safe_connection(*pool);

            return _command(cmd, safe_connection.connection(), std::forward<Args>(args)...);
        } catch (const IoError &) {
            // When master is down, one of its replicas will be promoted to be the new master.
            // If we try to send command to the old master, we'll get an *IoError*.
            // In this case, we need to update the slots mapping.
            _pool->update();
        } catch (const ClosedError &) {
            // Node might be removed.
            // 1. Get up-to-date slot mapping to check if the node still exists.
            _pool->update();

            // TODO:
            // 2. If it's NOT exist, update slot mapping, and retry.
            // 3. If it's still exist, that means the node is down, NOT removed, throw exception.
        } catch (const MovedError &) {
            // Slot mapping has been changed, update it and try again.
            _pool->update();
        } catch (const AskError &err) {
            auto pool = _pool->fetch(err.node());
            assert(pool);
            SafeConnection safe_connection(*pool);
            auto &connection = safe_connection.connection();

            // 1. send ASKING command.
            _asking(connection);

            // 2. resend last command.
            try {
                return _command(cmd, connection, std::forward<Args>(args)...);
            } catch (const MovedError &) {
                throw Error("Slot migrating... ASKING node hasn't been set to IMPORTING state");
            }
        } // For other exceptions, just throw it.
    }

    // Possible failures:
    // 1. Source node has already run 'CLUSTER SETSLOT xxx NODE xxx',
    //    while the destination node has NOT run it.
    //    In this case, client will be redirected by both nodes with MovedError.
    // 2. Node is down, e.g. master is down, and new master has not been elected yet.
    // 3. Other failures...
    throw Error("Failed to send command with key: " + std::string(key.data(), key.size()));
}

template <typename Cmd, typename ...Args>
inline ReplyUPtr RedisCluster::_score_command(std::true_type, Cmd cmd, Args &&... args) {
    return command(cmd, std::forward<Args>(args)..., true);
}

template <typename Cmd, typename ...Args>
inline ReplyUPtr RedisCluster::_score_command(std::false_type, Cmd cmd, Args &&... args) {
    return command(cmd, std::forward<Args>(args)..., false);
}

template <typename Output, typename Cmd, typename ...Args>
inline ReplyUPtr RedisCluster::_score_command(Cmd cmd, Args &&... args) {
    return _score_command(typename IsKvPairIter<Output>::type(),
                            cmd,
                            std::forward<Args>(args)...);
}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_REDIS_CLUSTER_HPP
