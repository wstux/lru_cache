/*
 * The MIT License
 *
 * Copyright 2019 Chistyakov Alexander.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _LRU_CACHE_THREAD_SAFE_LRU_CACHE_H
#define _LRU_CACHE_THREAD_SAFE_LRU_CACHE_H

#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>
#include <utility>

#include "lru_cache/lru_cache.h"
#include "lru_cache/spinlock.h"

namespace wstux {
namespace cnt {

template<typename TKey, typename TValue,
         template<typename THType> class THasher = std::hash,
         template<typename TMKey, typename TMVal, typename TMHash> class TMap = std::unordered_map,
         template<typename TLType> class TList = std::list,
         class TLock = std::mutex>
class thread_safe_lru_cache
{
    typedef lru_cache<TKey, TValue, THasher, TMap, TList>   _shard_type;
    typedef std::shared_ptr<_shard_type>                    _shard_ptr_type;
    typedef std::pair<_shard_ptr_type, TLock>               _safe_shard_type;

public:
    typedef typename _shard_type::key_type          key_type;
    typedef typename _shard_type::value_type        value_type;
    typedef typename _shard_type::size_type         size_type;
    typedef typename _shard_type::hasher            hasher;
    typedef typename _shard_type::reference         reference;
    typedef typename _shard_type::const_reference   const_reference;
    typedef typename _shard_type::pointer           pointer;
    typedef typename _shard_type::const_pointer     const_pointer;
                                            
    explicit thread_safe_lru_cache(size_t capacity, size_t shards_count)
        : m_capacity(capacity)
        , m_shards((m_capacity > shards_count) ? shards_count : m_capacity)
    {
        shards_count = m_shards.size();
        for (size_t i = 0; i < shards_count; i++) {
            const size_t shard_capacity = (i != 0)
                ? (m_capacity / shards_count)
                : ((m_capacity / shards_count) + (m_capacity % shards_count));
            m_shards[i].first = std::make_shared<_shard_type>(shard_capacity);
        }
    }

    thread_safe_lru_cache(const thread_safe_lru_cache&) = delete;
    thread_safe_lru_cache& operator=(const thread_safe_lru_cache&) = delete;

    bool find(const key_type& key, value_type& result)
    {
        return wrapper(get_shard(key), &_shard_type::find, key, result);
    }

    bool insert(const key_type& key, const value_type& val)
    {
        return wrapper(get_shard(key), &_shard_type::insert, key, val);
    }

    void update(const key_type& key, const value_type& val)
    {
        wrapper(get_shard(key), &_shard_type::update, key, val);
    }

//    void update(const key_type& key, value_type&& val)
//    {
//        wrapper(get_shard(key), &_shard_type::update, key, val);
//    }

private:
    inline _safe_shard_type& get_shard(const TKey& key)
    {
        constexpr size_t shift = std::numeric_limits<size_t>::digits - 16;
        const size_t h = (hasher{}(key) >> shift) % m_shards.size();
        return m_shards[h];
    }

    template<typename T, typename TFn, typename... TArgs>
    inline typename std::result_of<TFn(_shard_type, TArgs...)>::type wrapper(T& p, const TFn& func, TArgs&&... args)
    {
        std::unique_lock<decltype(p.second)> lock(p.second);
        return (p.first.get()->*func)(std::forward<TArgs>(args)...);
    }

private:
    const size_t m_capacity;
    std::vector<_safe_shard_type> m_shards;
};

} // namespace cnt
} // namespace wstux

#endif /* _LRU_CACHE_THREAD_SAFE_LRU_CACHE_H */
