/*
 * MIT License
 *
 * Copyright (c) 2025 Malaka Gunawardana
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once
#include <core/memory.h>
#include <stdint.h>

/**
 * class LinkedList - A singly-linked list with head and tail tracking.
 * @head: First node in the list, or null when empty.
 * @tail: Last node in the list, or null when empty.
 * @size: Number of elements currently in the list.
 */
template <typename T>
class LinkedList {
private:
    /**
     * struct Node - One list element.
     * @data: Stored element.
     * @next: Following node, or null at the tail.
     */
    struct Node {
        T data;
        Node* next;
        Node(const T& _data) : data(_data), next(nullptr) {}
    };

    Node* head;
    Node* tail;
    uint32_t size;

    void CopyFrom(const LinkedList& other) {
        Node* current = other.head;
        while (current) {
            PushBack(current->data);
            current = current->next;
        }
    }

public:
    /**
     * LinkedList() - Construct an empty list.
     */
    LinkedList() : head(nullptr), tail(nullptr), size(0) {}

    /**
     * ~LinkedList() - Destroy the list and free every node.
     */
    ~LinkedList() {
        Clear();
    }

    /**
     * LinkedList() - Copy-construct from another list.
     * @other: List whose elements are copied.
     */
    LinkedList(const LinkedList& other) : head(nullptr), tail(nullptr), size(0) {
        CopyFrom(other);
    }

    /**
     * operator=() - Replace the list contents with another list's elements.
     * @other: List whose elements are copied.
     *
     * Return: A reference to this list.
     */
    LinkedList& operator=(const LinkedList& other) {
        if (this != &other) {
            Clear();
            CopyFrom(other);
        }
        return *this;
    }

    /**
     * Add() - Prepend an element to the head of the list.
     * @item: Element to insert.
     */
    void Add(const T& item) {
        Node* newNode = new Node(item);
        newNode->next = head;
        head = newNode;
        if (!tail) tail = head;
        size++;
    }

    /**
     * PushBack() - Append an element to the tail of the list.
     * @item: Element to insert.
     */
    void PushBack(const T& item) {
        Node* newNode = new Node(item);
        if (!tail) {
            head = tail = newNode;
        } else {
            tail->next = newNode;
            tail = newNode;
        }
        size++;
    }

    /**
     * Find() - Return the first element satisfying a condition.
     * @condition: Predicate invoked with each element.
     *
     * Return: The first matching element, or a default-constructed T.
     */
    template <typename Func>
    T Find(Func condition) const {
        Node* current = head;
        while (current) {
            if (condition(current->data)) {
                return current->data;
            }
            current = current->next;
        }
        return T{};
    }

    /**
     * Remove() - Remove the first element satisfying a condition.
     * @condition: Predicate invoked with each element.
     *
     * Return: True when an element was removed.
     */
    template <typename Func>
    bool Remove(Func condition) {
        Node* current = head;
        Node* prev = nullptr;

        while (current) {
            if (condition(current->data)) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    head = current->next;
                }
                if (current == tail) {
                    tail = prev;
                }
                delete current;
                size--;
                return true;
            }
            prev = current;
            current = current->next;
        }
        return false;
    }

    /**
     * GetSize() - Report the number of elements.
     *
     * Return: The element count.
     */
    uint32_t GetSize() const {
        return size;
    }

    /**
     * IsEmpty() - Check whether the list holds no elements.
     *
     * Return: True when the list is empty.
     */
    bool IsEmpty() const {
        return size == 0;
    }

    /**
     * ForEach() - Invoke a function on every element, in order.
     * @func: Callback receiving each element.
     */
    template <typename Func>
    void ForEach(Func func) const {
        Node* current = head;
        while (current) {
            func(current->data);
            current = current->next;
        }
    }

    /**
     * Take() - Remove and return the first element satisfying a condition.
     * @condition: Predicate invoked with each element.
     *
     * Return: The removed element, or a default-constructed T.
     */
    template <typename Func>
    T Take(Func condition) {
        Node* current = head;
        Node* prev = nullptr;

        while (current) {
            if (condition(current->data)) {
                T result = current->data;

                if (prev) {
                    prev->next = current->next;
                } else {
                    head = current->next;
                }
                if (current == tail) {
                    tail = prev;
                }

                delete current;
                size--;
                return result;
            }

            prev = current;
            current = current->next;
        }

        return T{};
    }

    /**
     * PopFront() - Remove and return the head element.
     *
     * Return: The removed element, or a default-constructed T.
     */
    T PopFront() {
        if (!head) return T{};
        Node* temp = head;
        T result = head->data;
        head = head->next;
        if (!head) tail = nullptr;
        delete temp;
        size--;
        return result;
    }

    /**
     * GetFront() - Return the head element without removing it.
     *
     * Return: The head element, or a default-constructed T.
     */
    T GetFront() {
        if (!head) return T{};
        T result = head->data;
        return result;
    }

    /**
     * Clear() - Delete every node and reset the list to empty.
     */
    void Clear() {
        Node* current = head;
        while (current) {
            Node* temp = current;
            current = current->next;
            delete temp;
        }
        head = tail = nullptr;
        size = 0;
    }

    // ----------- Iterator support -----------
    /**
     * class Iterator - Forward iterator over the list elements.
     */
    class Iterator {
    private:
        Node* current;

    public:
        Iterator(Node* start) : current(start) {}

        /**
         * operator*() - Dereference the current element.
         *
         * Return: Reference to the current element.
         */
        T& operator*() const {
            return current->data;
        }

        /**
         * operator++() - Advance to the next element.
         *
         * Return: Reference to this iterator.
         */
        Iterator& operator++() {
            if (current) current = current->next;
            return *this;
        }

        /**
         * operator!=() - Compare two iterators for inequality.
         * @other: Iterator to compare against.
         *
         * Return: True when the iterators point at different nodes.
         */
        bool operator!=(const Iterator& other) const {
            return current != other.current;
        }
    };

    /**
     * begin() - Return an iterator at the head of the list.
     *
     * Return: An iterator over the first element.
     */
    Iterator begin() const {
        return Iterator(head);
    }

    /**
     * end() - Return the past-the-end iterator.
     *
     * Return: An iterator representing the end of the list.
     */
    Iterator end() const {
        return Iterator(nullptr);
    }

    /**
     * ReverseForEach() - Invoke a function on each element in reverse order.
     * @func: Callback receiving each element.
     */
    template <typename Func>
    void ReverseForEach(Func func) const {
        T* stack[128];  // Fixed-size stack.
        uint32_t count = 0;

        Node* current = head;
        while (current && count < 128) {
            stack[count++] = &current->data;
            current = current->next;
        }

        for (int32_t i = count - 1; i >= 0; --i) {
            func(*stack[i]);
        }
    }
};
