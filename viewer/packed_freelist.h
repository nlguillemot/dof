#pragma once

// self-packing freelist implementation based on http://bitsquid.blogspot.ca/2011/09/managing-decoupling-part-4-id-lookup.html
// has NOT been unit tested. beware of using in production.

#include <cstdint>
#include <cassert>
#include <utility>

template<class T>
class packed_freelist
{
    // used to extract the allocation index from an object id
    static const uint16_t alloc_index_mask = 0xFFFF;

    // used to mark an allocation as owning no object
    static const uint16_t tombstone = 0xFFFF;

    struct allocation_t
    {
        // the ID of this allocation
        // * The 16 LSBs store the index of this allocation in the list of allocations
        // * The 16 MSBs store the number of times this allocation struct was used to allocate an object
        //      * this is used as a (non-perfect) counter-measure to reusing IDs for objects
        uint32_t allocation_id;

        // the index in the objects array which stores the allocated object for this allocation
        uint16_t object_index;

        // the index in the allocations array for the next allocation to allocate after this one
        uint16_t next_allocation;
    };

    // Storage for objects
    // Objects are contiguous, and always packed to the start of the storage.
    // Objects can be relocated in this storage thanks to the separate list of allocations.
    size_t _num_objects;
    size_t _max_objects;
    size_t _cap_objects;
    T* _objects;
    
    // the allocation ID of each object in the object array (1-1 mapping)
    uint32_t* _object_alloc_ids;

    // FIFO queue to allocate objects with least ID reuse possible
    allocation_t* _allocations;

    // when an allocation is freed, the enqueue index struct's next will point to it
    // this ensures that allocations are reused as infrequently as possible,
    // which reduces the likelihood that two objects have the same ID.
    // note objects are still not guaranteed to have globally unique IDs, since IDs will be reused after N * 2^16 allocations
    uint16_t _last_allocation;
    
    // the next index struct to use for an allocation
    uint16_t _next_allocation;

public:
    struct iterator
    {
        iterator(uint32_t* in)
        {
            _curr_object_alloc_id = in;
        }

        iterator& operator++()
        {
            _curr_object_alloc_id++;
            return *this;
        }

        uint32_t operator*()
        {
            return *_curr_object_alloc_id;
        }

        bool operator!=(const iterator& other) const
        {
            return _curr_object_alloc_id != other._curr_object_alloc_id;
        }

    private:
        uint32_t* _curr_object_alloc_id;
    };

    packed_freelist()
    {
        _num_objects = 0;
        _max_objects = 0;
        _cap_objects = 0;
        _objects = nullptr;
        _object_alloc_ids = nullptr;
        _allocations = nullptr;
        _last_allocation = -1;
        _next_allocation = -1;
    }

    packed_freelist(size_t max_objects)
    {
        // -1 because index 0xFFFF is reserved as a tombstone
        assert(max_objects < 0x10000 - 1);

        _num_objects = 0;
        _max_objects = max_objects;
        _cap_objects = max_objects;

        _objects = (T*)new char[max_objects * sizeof(T)];
        assert(_objects);

        _object_alloc_ids = (uint32_t*)new uint32_t[max_objects];
        assert(_object_alloc_ids);

        _allocations = new allocation_t[max_objects];
        assert(_allocations);

        for (size_t i = 0; i < max_objects; i++)
        {
            _allocations[i].allocation_id = (uint32_t)i;
            _allocations[i].object_index = tombstone;
            _allocations[i].next_allocation = (uint16_t)(i + 1);
        }
        
        if (max_objects > 0)
            _allocations[max_objects - 1].next_allocation = 0;

        _last_allocation = (uint16_t)(max_objects - 1);
        _next_allocation = 0;
    }

    ~packed_freelist()
    {
        for (size_t i = 0; i < _num_objects; i++)
        {
            _objects[i].~T();
        }
        delete[] ((char*)_objects);
        delete[] _object_alloc_ids;
        delete[] _allocations;
    }

    packed_freelist(const packed_freelist& other)
    {
        _num_objects = other._num_objects;
        _max_objects = other._max_objects;
        _cap_objects = other._cap_objects;
        
        _objects = (T*)new char[other._max_objects * sizeof(T)];
        assert(_objects);

        _object_alloc_ids = new uint32_t[other._max_objects];
        assert(_object_alloc_ids);

        _allocations = new allocation_t[other._max_objects];
        assert(_allocations);

        for (size_t i = 0; i < other._num_objects; i++)
        {
            new (_objects + i) T(*(other._objects + i));
            _object_alloc_ids[i] = other._object_alloc_ids[i];
        }

        for (size_t i = 0; i < other._max_objects; i++)
        {
            _allocations[i] = other._allocations[i];
        }

        _last_allocation = other._last_allocation;
        _next_allocation = other._next_allocation;
    }

    packed_freelist& operator=(const packed_freelist& other)
    {
        if (this != &other)
        {
            if (_cap_objects < other._max_objects)
            {
                this->~packed_freelist();
                new (this) packed_freelist(other);
            }
            else
            {
                for (size_t i = 0; i < other._num_objects; i++)
                {
                    if (i < _num_objects)
                    {
                        *(_objects + i) = *(other._objects + i);
                    }
                    else
                    {
                        new (_objects + i) T(*(other._objects + i));
                    }
                    _object_alloc_ids[i] = other._object_alloc_ids[i];
                }

                for (size_t i = 0; i < other._max_objects; i++)
                {
                    _allocations[i] = other._allocations[i];
                }

                _num_objects = other._num_objects;
                _max_objects = other._max_objects;
                _last_allocation = other._last_allocation;
                _next_allocation = other._next_allocation;
            }
        }
        return *this;
    }

    void swap(packed_freelist& other)
    {
        using std::swap;
        swap(_num_objects, other._num_objects);
        swap(_max_objects, other._max_objects);
        swap(_cap_objects, other._cap_objects);
        swap(_objects, other._objects);
        swap(_object_alloc_ids, other._object_alloc_ids);
        swap(_allocations, other._allocations);
        swap(_last_allocation, other._last_allocation);
        swap(_next_allocation, other._next_allocation);
    }

    packed_freelist(packed_freelist&& other)
        : packed_freelist()
    {
        swap(other);
    }

    packed_freelist& operator=(packed_freelist&& other)
    {
        if (this != &other)
        {
            swap(other);
        }
        return *this;
    }

    bool contains(uint32_t id) const
    {
        // grab the allocation by grabbing the allocation index from the id's LSBs
        allocation_t* alloc = &_allocations[id & alloc_index_mask];

        // * NON-conservative test that the IDs match (if the allocation has been reused 2^16 times, it'll loop over)
        // * Also check that the object is hasn't already been deallocated.
        //      * This'll prevent an object that was just freed from appearing to be contained, but still doesn't disambiguate between two objects with the same ID (see first bullet point)
        return alloc->allocation_id == id && alloc->object_index != tombstone;
    }

    T& operator[](uint32_t id) const
    {
        // grab the allocation corresponding to this ID
        allocation_t* alloc = &_allocations[id & alloc_index_mask];

        // grab the object associated with the allocation
        return *(_objects + (alloc->object_index));
    }

    uint32_t insert(const T& val)
    {
        allocation_t* alloc = insert_alloc();
        T* o = _objects + alloc->object_index;
        new (o) T(val);
        return alloc->allocation_id;
    }

    uint32_t insert(T&& val)
    {
        allocation_t* alloc = insert_alloc();
        T* o = _objects + alloc->object_index;
        new (o) T(std::move(val));
        return alloc->allocation_id;
    }

    template<class... Args>
    uint32_t emplace(Args&&... args)
    {
        allocation_t* alloc = insert_alloc();
        T* o = _objects + alloc->object_index;
        new (o) T(std::forward<Args>(args)...);
        return alloc->allocation_id;
    }

    void erase(uint32_t id)
    {
        assert(contains(id));

        // grab the allocation to delete
        allocation_t* alloc = &_allocations[id & alloc_index_mask];

        // grab the object for this allocation
        T* o = _objects + alloc->object_index;

        // if necessary, move (aka swap) the last object into the location of the object to erase, then unconditionally delete the last object
        if (alloc->object_index != _num_objects - 1)
        {
            T* last = _objects + _num_objects - 1;
            *o = std::move(*last);
            o = last;

            // since the last object was moved into the deleted location, the associated object ID array's value must also be moved similarly
            _object_alloc_ids[alloc->object_index] = _object_alloc_ids[_num_objects - 1];

            // since the last object has changed location, its allocation needs to be updated to the new location.
            _allocations[_object_alloc_ids[alloc->object_index] & alloc_index_mask].object_index = alloc->object_index;
        }

        // destroy the removed object and pop it from the array
        o->~T();
        _num_objects = _num_objects - 1;

        // push the deleted allocation onto the FIFO
        _allocations[_last_allocation].next_allocation = alloc->allocation_id & alloc_index_mask;
        _last_allocation = alloc->allocation_id & alloc_index_mask;

        // put a tombstone where the allocation used to point to an object index
        alloc->object_index = tombstone;
    }

    iterator begin() const
    {
        return iterator{ _object_alloc_ids };
    }

    iterator end() const
    {
        return iterator{ _object_alloc_ids + _num_objects };
    }

    bool empty() const
    {
        return _num_objects == 0;
    }

    size_t size() const
    {
        return _num_objects;
    }

    size_t capacity() const
    {
        return _max_objects;
    }

private:
    allocation_t* insert_alloc()
    {
        assert(_num_objects < _max_objects);

        // pop an allocation from the FIFO
        allocation_t* alloc = &_allocations[_next_allocation];
        _next_allocation = alloc->next_allocation;

        // increment the allocation count in the 16 MSBs without modifying the allocation's index (in the 16 LSBs)
        alloc->allocation_id += 0x10000;
        
        // always allocate the object at the end of the storage
        alloc->object_index = (uint16_t)_num_objects;
        _num_objects = _num_objects + 1;

        // update reverse-lookup so objects can know their ID
        _object_alloc_ids[alloc->object_index] = alloc->allocation_id;

        return alloc;
    }
};

template<class T>
typename packed_freelist<T>::iterator begin(const packed_freelist<T>& fl)
{
    return fl.begin();
}

template<class T>
typename packed_freelist<T>::iterator end(const packed_freelist<T>& fl)
{
    return fl.end();
}

template<class T>
void swap(packed_freelist<T>& a, packed_freelist<T>& b)
{
    a.swap(b);
}