//push()的第一次修订(不正确的)
#include <memory>
#include <atomic>

template<typename T>
class lock_free_queue {
private:
    struct node {
        std::shared_ptr<T> data;
        node *next;

        node() :
                next(nullptr) {}
    };

    std::atomic<node *> head;
    std::atomic<node *> tail;

    node *pop_head() {
        node *const old_head = head.load();     // 1
        if (old_head == tail.load()) {
            return nullptr;
        }
        head.store(old_head->next);
        return old_head;
    }

public:
    lock_free_queue() :
            head(new node), tail(head.load()) {}

    lock_free_queue(const lock_free_queue &other) = delete;

    lock_free_queue &operator=(const lock_free_queue &other) = delete;

    ~lock_free_queue() {
        while (node *const old_head = head.load()) {
            head.store(old_head->next);
            delete old_head;
        }
    }

    std::shared_ptr<T> pop() {
        node *old_head = pop_head();
        if (!old_head) {
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> const res(old_head->data);           // 2
        delete old_head;
        return res;
    }

    void push(T new_value) {
        std::unique_ptr<T> new_data(new T(new_value));
        counted_node_ptr new_next;
        new_next.ptr = new node;
        new_next.external_count = 1;
        for (;;) {
            node *const old_tail = tail.load();             // 1
            T *old_data = nullptr;
            if (old_tail->data.compare_exchange_strong(old_data, new_data.get())) {     // 2
                old_tail->next = new_next;
                tail.store(new_next.ptr);                   // 3
                new_data.release();
                break;
            }
        }
    }
};

int main() {}

/**
 * 使用引用计数方案可以避免竞争，不过竞争不只在push()中。可以再看一下7.14中的修订版
push()，与栈中模式相同：加载一个原子指针①，并且对该指针解引用②。同时，另一个线程
可以对指针进行更新③，最终回收该节点(在pop()中)。当节点回收后，再对指针进行解引用，
就对导致未定义行为。啊哈！这里有个诱人的方案，就是给tail也添加计数器，就像给head做
的那样，不过队列中的节点的next指针中都已经拥有了一个外部计数。在同一个节点上有两个
外部计数，为了避免过早的删除节点，这就是对之前引用计数方案的修改。通过对node结构
中外部计数器数量的统计，解决这个问题。当外部计数器销毁时，统计值减一(将对应的外部
计数添加到内部)。当内部计数是0，且没有外部计数器时，对应节点就可以被安全删除了。
 *
 *
 *
 */