//
// Created by schrodinger on 1/15/21.
//

#ifndef GRAPH_COLORING_HEAP_H
#define GRAPH_COLORING_HEAP_H

#include <vector>
#include <algorithm>
#include <functional>

/*!
 * The DecHeap class. Using binary heap as data structure.
 */
class DecHeap {
    /*!
     * The binary heap. We store also the index of the number to enable
     * the decreasing capability.
     */
    std::vector <std::pair<size_t, size_t>> heap{};
    /*!
     * The index map. Mapping index to real register index.
     */
    std::vector <size_t> idx_map{};

    /*!
     * Heapify operation
     * @param idx the index of the element to trickle down.
     */
    void trickle_down(size_t idx);

    /*!
     * Heapify operation
     * @param idx idx the index of the element to bubble up.
     */
    void bubble_up(size_t idx);

    /*!
     * Swap element and maintain the index referencing relation.
     * @param a element to be swap
     * @param b element to be swap
     */
    void swap(size_t a, size_t b);

public:
    /*!
     * DecHeap Constructor
     * @param heap the set of numbers to be made into the heap
     */
    explicit DecHeap(const std::vector <size_t> &heap);

    /*!
     * Decrease element by delta
     * @param node element reference index
     * @param delta amount to be decreased
     */
    void decrease(size_t node, size_t delta);

    /*!
     * Pop the minimal element
     * @return
     */
    std::pair <size_t, size_t> pop();

    /*!
     * Check whether the heap is empty
     * @return the check result
     */
    bool empty() const;
};


#endif //GRAPH_COLORING_HEAP_H
