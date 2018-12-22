//获取整个threadsafe_lookup_table作为一个 std::map<>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <list>
#include <utility>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/mutex.hpp>

template<typename Key, typename Value, typename Hash=std::hash<Key> >
class threadsafe_lookup_table {
private:
    class bucket_type {
    private:
        typedef std::pair<Key, Value> bucket_value;
        typedef std::list<bucket_value> bucket_data;
        typedef typename bucket_data::iterator bucket_iterator;

        bucket_data data;
        mutable boost::shared_mutex mutex;                          // 1

        bucket_iterator find_entry_for(Key const &key) const {      // 2
            return std::find_if(data.begin(), data.end(), [&](bucket_value const &item) { return item.first == key; });
        }

    public:
        Value value_for(Key const &key, Value const &default_value) const {
            boost::shared_lock<boost::shared_mutex> lock(mutex);      // 3
            bucket_iterator const found_entry = find_entry_for(key);
            return (found_entry == data.end()) ? default_value : found_entry->second;
        }

        void add_or_update_mapping(Key const &key, Value const &value) {
            std::unique_lock<boost::shared_mutex> lock(mutex);        // 4
            bucket_iterator const found_entry = find_entry_for(key);
            if (found_entry == data.end()) {
                data.push_back(bucket_value(key, value));
            } else {
                found_entry->second = value;
            }
        }

        void remove_mapping(Key const &key) {
            std::unique_lock<boost::shared_mutex> lock(mutex);      // 5
            bucket_iterator const found_entry = find_entry_for(key);
            if (found_entry != data.end()) {
                data.erase(found_entry);
            }
        }
    };

    std::vector<std::unique_ptr<bucket_type> > buckets;             // 6
    Hash hasher;

    bucket_type &get_bucket(Key const &key) const {               // 7
        std::size_t const bucket_index = hasher(key) % buckets.size();
        return *buckets[bucket_index];
    }

public:
    typedef Key key_type;
    typedef Value mapped_type;
    typedef Hash hash_type;

    threadsafe_lookup_table(unsigned num_buckets = 19, Hash const &hasher_ = Hash()) :
            buckets(num_buckets),
            hasher(hasher_) {
        for (unsigned i = 0; i < num_buckets; ++i) {
            buckets[i].reset(new bucket_type);
        }
    }

    threadsafe_lookup_table(threadsafe_lookup_table const &other) = delete;

    threadsafe_lookup_table &operator=(threadsafe_lookup_table const &other) = delete;

    Value value_for(Key const &key, Value const &default_value = Value()) const {
        return get_bucket(key).value_for(key, default_value);               // 8
    }

    void add_or_update_mapping(Key const &key, Value const &value) {
        get_bucket(key).add_or_update_mapping(key, value);                  // 9
    }

    void remove_mapping(Key const &key) {
        get_bucket(key).remove_mapping(key);                                // 10
    }

    std::map<Key, Value> get_map() const {
        std::vector<std::unique_lock<boost::shared_mutex> > locks;
        for (unsigned i = 0; i < buckets.size(); ++i) {
            locks.push_back(std::unique_lock<boost::shared_mutex>(buckets[i].mutex));
        }
        std::map<Key, Value> res;
        for (unsigned i = 0; i < buckets.size(); ++i) {
            for (bucket_iterator it = buckets[i].data.begin(); it != buckets[i].data.end(); ++it) {
                res.insert(*it);
            }
        }
        return res;
    }
};

int main() {}

/**
 * get_map函数:查询表实现就增大的并发访问的可能性，这个查询表作为一个整体，通过单独的操作对每一个桶进行锁定，并且通过使用boost::shared_mutex允许读者线程对每一个
 * 桶进行并发访问。
 *
 */
