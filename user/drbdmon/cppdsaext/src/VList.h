/**
 * Vector/List
 *
 * @version 2016-03-21_001
 * @author  Robert Altnoeder (r.altnoeder@gmx.net)
 *
 * Copyright (C) 2012 - 2016 Robert ALTNOEDER
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided that
 * the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef VLIST_H
#define	VLIST_H

template<typename V>
class VList
{
    typedef int (*compare_func)(const V* value, const V* other);

  public:
    class Node
    {
        friend class VList;

      public:
        Node(V* value_ref):
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
            return value;
        }

        virtual void reuse()
        {
            next  = nullptr;
            prev  = nullptr;
            value = nullptr;
        }

      private:
        Node* next  {nullptr};
        Node* prev  {nullptr};
        V*    value {nullptr};
    };

    template<typename T>
    class BaseIterator : public dsaext::QIterator<T>
    {
      public:
        BaseIterator(const VList<T>& vlist_ref):
            vlist_obj(vlist_ref)
        {
            iter_node = vlist_obj.head;
        }

        virtual ~BaseIterator() = default;
        BaseIterator(const BaseIterator& orig) = default;
        BaseIterator& operator=(const BaseIterator& orig) = default;
        BaseIterator(BaseIterator&& orig) = default;
        BaseIterator& operator=(BaseIterator&& orig) = default;

        virtual T* next() = 0;

        virtual size_t get_size() const
        {
            return vlist_obj.size;
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
        const VList<T>& vlist_obj;
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
            V* iter_value {nullptr};
            Node* node = BaseIterator<V>::next_node();
            if (node != nullptr)
            {
                iter_value = node->value;
            }
            return iter_value;
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

    virtual V* get(V* value_ptr)
    {
        V* value = nullptr;
        Node* node = find_node(value_ptr);
        if (node != nullptr)
        {
            value = node->value;
        }
        return value;
    }

    virtual Node* get_node(V* value_ptr)
    {
        return find_node(value_ptr);
    }

    // @throw std::bad_alloc
    virtual void prepend(V* value_ptr)
    {
        Node* ins_node = new Node(value_ptr);
        prepend_node_impl(ins_node);
    }

    // @throw std::bad_alloc
    virtual void append(V* value_ptr)
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
    virtual void insert_before(Node* node, V* value_ptr)
    {
        Node* ins_node = new Node(value_ptr);
        insert_node_before_impl(node, ins_node);
    }

    virtual void insert_node_before(Node* node, Node* ins_node)
    {
        insert_node_before_impl(node, ins_node);
    }

    virtual void remove(V* value_ptr)
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
    Node* find_node(V* value_ptr)
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

    compare_func compare;
};

#endif	/* VLIST_H */
