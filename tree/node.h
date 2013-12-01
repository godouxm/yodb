#ifndef _YODB_NODE_H_
#define _YODB_NODE_H_

#include "tree/msg.h"
#include "sys/rwlock.h"
#include "util/timestamp.h"
#include "util/slice.h"
#include "util/logger.h"

#include <stdint.h>
#include <vector>

namespace yodb {

typedef uint64_t nid_t; 

#define NID_NIL     ((nid_t)0)

class BufferTree;

class Pivot {
public:
    Pivot() {}
    Pivot(nid_t child, MsgBuf* mbuf, Slice key = Slice())
        : msgbuf(mbuf), child_nid(child), left_most_key_(key) {}

    Slice left_most_key() 
    {
        // when left_most_key() function is called, 
        // the left_most_key must be exists(Actually, bad design).
        assert(left_most_key_.size());
        return left_most_key_;
    }

    MsgBuf* msgbuf;
    nid_t child_nid;
private:
    Slice left_most_key_;
};

class Node {
public:
    Node(BufferTree* tree, nid_t self)
        : tree_(tree), self_nid_(self), parent_nid_(NID_NIL), 
          node_page_size_(0), pivots_(), rwlock_(), mutex_(),
          modified_(false), flushing_(false)
    {
    }

    bool get(const Slice& key, Slice& value);

    bool put(const Slice& key, const Slice& value)
    {
        return write(Msg(Put, key.clone(), value.clone()));
    }

    bool del(const Slice& key)
    {
        return write(Msg(Del, key.clone()));
    }
    
    size_t size() { return node_page_size_; }
    size_t write_back_size();

    bool write(const Msg& msg);

    // when the leaf node's number of pivot is out of limit,
    // it then will split the node and push up the split operation.
    void try_split_node(std::vector<nid_t>& path);

    void create_first_pivot();
    // find which pivot matches the key
    size_t find_pivot(Slice key);
    void add_pivot(Slice key, nid_t child, nid_t child_sibling);

    // maybe push down or split the msgbuf
    void maybe_push_down_or_split();

    // internal node would push down the msgbuf when it is full 
    void push_down_msgbuf(MsgBuf* msgbuf, nid_t parent);

    // only the leaf node would split msgbuf when it is full
    void split_msgbuf(MsgBuf* msgbuf);

    typedef std::vector<Pivot> Container;

    void lock_path(const Slice& key, std::vector<nid_t>& path);
    void push_down_during_lock_path(MsgBuf* msgbuf);

    bool try_read_lock()    { return rwlock_.try_read_lock(); }
    bool try_write_lock()   { return rwlock_.try_write_lock(); }

    void read_lock()        { rwlock_.read_lock(); }
    void read_unlock()      { rwlock_.read_unlock(); }

    void write_lock()       { rwlock_.write_lock(); }
    void write_unlock()     { rwlock_.write_unlock(); }


    void set_modify(bool modified)
    {
        ScopedMutex lock(mutex_);

        if (!modified_ && modified) 
            first_write_timestamp_ = Timestamp::now();

        modified_ = modified;
    }

    bool modified() 
    {
        ScopedMutex lock(mutex_);
        return modified_;
    }

    void set_flushing(bool flushing)
    {
        ScopedMutex lock(mutex_);
        flushing_ = flushing;
    }

    bool flushing()
    {
        ScopedMutex lock(mutex_);
        return flushing_;
    }

    Timestamp get_first_write_timestamp()
    {
        ScopedMutex lock(mutex_);
        return first_write_timestamp_;
    }

    Timestamp get_last_used_timestamp()
    {
        ScopedMutex lock(mutex_);
        return last_used_timestamp_;
    }

    bool constrcutor(const BlockReader& reader);
    bool destructor(BlockWriter& writer);

private:
    // set public to make current work(including test) more easier.
public:
    BufferTree* tree_;
    nid_t self_nid_;
    nid_t parent_nid_;
    bool is_leaf_;

    size_t node_page_size_;
    Container pivots_; 
    RWLock rwlock_;

    Mutex mutex_;
    bool modified_;
    bool flushing_;
    Timestamp first_write_timestamp_;
    Timestamp last_used_timestamp_;
};

} // namespace yodb

#endif // _YODB_NODE_H_
