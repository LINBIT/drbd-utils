/**
 * From https://github.com/raltnoeder/cppdsaext
 * Included under the GPLv2 license. See source project for other licensing options.
 *
 * Vector/List, implemented as a double ended queue (deque)
 *
 * @version 2018-05-16_001
 * @author  Robert Altnoeder (r.altnoeder@gmx.net)
 *
 * Copyright (C) 2012 - 2018 Robert ALTNOEDER
 */
#ifndef VLIST_H
#define	VLIST_H

#include <cstddef>
#include <new>

#include <dsaext.h>

template<typename V>
class VList
{
    typedef int (*compare_func)(const V* value, const V* other);

  public:
    class Node
    {
        friend class VList;

      public:
        Node(const V* value_ref):
            value(value_ref)
        {
        }

        Node(const Node& orig):
            value(orig.value)
        {
        }

        Node& operator=(const Node& orig)
        {
            if (this != &orig)
            {
                value = orig.value;
            }
            return *this;
        }

        Node(Node&& orig) = default;
        Node& operator=(Node&& orig) = default;

        virtual ~Node() = default;

        virtual V* get_value() const
        {
            return const_cast<V*> (value);
        }

        virtual void reuse()
        {
            next  = nullptr;
            prev  = nullptr;
            value = nullptr;
        }

      private:
        Node*       next    {nullptr};
        Node*       prev    {nullptr};
        const V*    value   {nullptr};
    };

    template<typename T>
    class BaseIterator : public dsaext::QIterator<T>
    {
      public:
        BaseIterator(const VList<V>& vlist_ref):
            vlist_obj(&vlist_ref)
        {
            iter_node = vlist_obj->head;
        }

        virtual ~BaseIterator() = default;
        BaseIterator(const BaseIterator& orig) = default;
        BaseIterator& operator=(const BaseIterator& orig) = default;
        BaseIterator(BaseIterator&& orig) = default;
        BaseIterator& operator=(BaseIterator&& orig) = default;

        virtual T* next() = 0;

        virtual size_t get_size() const
        {
            return vlist_obj->size;
        }

        virtual bool has_next() const
        {
            return iter_node != nullptr;
        }

      protected:
        virtual Node* next_node()
        {
            Node* ret_node = iter_node;
            if (iter_node != nullptr)
            {
                iter_node = iter_node->next;
            }
            return ret_node;
        }
      private:
        const VList<V>* vlist_obj;
        Node* iter_node {nullptr};
    };

    class NodesIterator : public BaseIterator<Node>
    {
      public:
        NodesIterator(const VList& vlist_ref):
            BaseIterator<Node>::BaseIterator(vlist_ref)
        {
        }

        NodesIterator(const NodesIterator& orig) = default;
        NodesIterator& operator=(const NodesIterator& orig) = default;
        NodesIterator(NodesIterator&& orig) = default;
        NodesIterator& operator=(NodesIterator&& orig) = default;

        virtual~NodesIterator() noexcept
        {
        }

        virtual Node* next()
        {
            return BaseIterator<Node>::next_node();
        }
    };

    class ValuesIterator : public BaseIterator<V>
    {
      public:
        ValuesIterator(const VList& vlist_ref):
            BaseIterator<V>::BaseIterator(vlist_ref)
        {
        }

        ValuesIterator(const ValuesIterator& orig) = default;
        ValuesIterator& operator=(const ValuesIterator& orig) = default;
        ValuesIterator(ValuesIterator&& orig) = default;
        ValuesIterator& operator=(ValuesIterator&& orig) = default;

        virtual ~ValuesIterator() noexcept
        {
        }

        virtual V* next()
        {
            const V* iter_value {nullptr};
            Node* node = BaseIterator<V>::next_node();
            if (node != nullptr)
            {
                iter_value = node->value;
            }
            return const_cast<V*> (iter_value);
        }
    };

    VList(compare_func compare_fn):
        compare(compare_fn)
    {
    }

    VList(const VList& orig) = delete;
    VList& operator=(const VList& orig) = delete;
    VList(VList&& orig) = default;
    VList& operator=(VList&& orig) = default;
    virtual ~VList()
    {
        clear_impl();
    }

    virtual void clear()
    {
        clear_impl();
        head = nullptr;
        tail = nullptr;
        size = 0;
    }

    virtual size_t get_size() const
    {
        return size;
    }

    virtual V* get(V* value_ptr)
    {
        const V* value = nullptr;
        Node* node = find_node(value_ptr);
        if (node != nullptr)
        {
            value = node->value;
        }
        return const_cast<V*> (value);
    }

    virtual V* get_head_value()
    {
        const V* value = nullptr;
        if (head != nullptr)
        {
            value = head->value;
        }
        return const_cast<V*> (value);
    }

    virtual V* get_tail_value()
    {
        const V* value = nullptr;
        if (tail != nullptr)
        {
            value = tail->value;
        }
        return const_cast<V*> (value);
    }

    virtual Node* get_node(const V* value_ptr)
    {
        return find_node(value_ptr);
    }

    virtual Node* get_head_node()
    {
        return head;
    }

    virtual Node* get_tail_node()
    {
        return tail;
    }

    // @throw std::bad_alloc
    virtual void prepend(const V* value_ptr)
    {
        Node* ins_node = new Node(value_ptr);
        prepend_node_impl(ins_node);
    }

    // @throw std::bad_alloc
    virtual void append(const V* value_ptr)
    {
        Node* ins_node = new Node(value_ptr);
        append_node_impl(ins_node);
    }

    virtual void prepend_node(Node* ins_node)
    {
        prepend_node_impl(ins_node);
    }

    virtual void append_node(Node* ins_node)
    {
        append_node_impl(ins_node);
    }

    // @throw std::bad_alloc
    virtual void insert_before(Node* node, const V* value_ptr)
    {
        Node* ins_node = new Node(value_ptr);
        insert_node_before_impl(node, ins_node);
    }

    virtual void insert_node_before(Node* node, Node* ins_node)
    {
        insert_node_before_impl(node, ins_node);
    }

    virtual void remove(const V* value_ptr)
    {
        Node* rm_node = find_node(value_ptr);
        if (rm_node != nullptr)
        {
            remove_node_impl(rm_node);
        }
    }

    virtual void remove_node(Node* rm_node)
    {
        remove_node_impl(rm_node);
    }

    virtual void unlink_node(Node* rm_node)
    {
        unlink_node_impl(rm_node);
    }

  private:
    Node* find_node(const V* value_ptr) const
    {
        Node* result_node = head;
        while (result_node != nullptr)
        {
            if (compare(value_ptr, result_node->value) == 0)
            {
                break;
            }
            result_node = result_node->next;
        }
        return result_node;
    }

    void remove_node_impl(Node* rm_node)
    {
        unlink_node_impl(rm_node);
        delete rm_node;
    }

    void unlink_node_impl(Node* rm_node)
    {
        if (head == rm_node)
        {
            head = rm_node->next;
        }
        else
        {
            rm_node->prev->next = rm_node->next;
        }
        if (tail == rm_node)
        {
            tail = rm_node->prev;
        }
        else
        {
            rm_node->next->prev = rm_node->prev;
        }
        --size;
    }

    void clear_impl()
    {
        Node* map_node = head;
        while (map_node != nullptr)
        {
            Node* next_node = map_node->next;
            delete map_node;
            map_node = next_node;
        }
    }

    void insert_node_before_impl(Node* node, Node* ins_node)
    {
        Node* prev_node = node->prev;
        ins_node->next = node;
        node->prev = ins_node;
        if (prev_node == nullptr)
        {
            head = ins_node;
        }
        else
        {
            ins_node->prev = prev_node;
            prev_node->next = ins_node;
        }
        ++size;
    }

    void prepend_node_impl(Node* ins_node)
    {
        Node* next_node = head;

        ins_node->next = next_node;
        if (next_node != nullptr)
        {
            next_node->prev = ins_node;
        }
        head = ins_node;

        if (tail == nullptr)
        {
            tail = ins_node;
        }
        ++size;
    }

    void append_node_impl(Node* ins_node)
    {
        Node* prev_node = tail;

        ins_node->prev = prev_node;
        if (prev_node != nullptr)
        {
            prev_node->next = ins_node;
        }
        tail = ins_node;

        if (head == nullptr)
        {
            head = ins_node;
        }
        ++size;
    }

    Node*  head {nullptr};
    Node*  tail {nullptr};
    size_t size {0};

    const compare_func compare;
};

#endif	/* VLIST_H */
