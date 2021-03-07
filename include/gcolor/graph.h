//
// Created by schrodinger on 1/15/21.
//

#ifndef GRAPH_COLORING_GRAPH_H
#define GRAPH_COLORING_GRAPH_H

#include "heap.h"
#include <vector>
#include <cstdint>
#include <stack>
/*!
 * To speed up, we use hardware clz instruction to get the least unused register;
 * therefore, we can use a 64-bit integer to record the bit mask information.
 */
using bitmask_t = uint64_t;

/*!
 * Set the i-th binary digit.
 * @param mask reference to the mask integer.
 * @param i index of the digit.
 */
void mark(bitmask_t &mask, size_t i);

/*!
 * Get the first free register (represented by zero digit).
 * @param mask reference to the mask integer.
 * @return free register index.
 */
size_t get(bitmask_t &mask);

/*!
 * The Graph class. Represents a group of unlimited register to be colored.
 */
class Graph {
    /*!
     * A vector stores the connection between the graph nodes.
     */
    std::vector <std::pair<size_t, size_t>> graph;
    /*!
     * Graph size.
     */
    size_t N;
public:
    /*!
     * Graph constructor.
     * @param g connection vector.
     * @param N graph size.
     */
    explicit Graph(std::vector <std::pair<size_t, size_t>> g, size_t N);

    /*!
     * Color the graph.
     * @param colors number of colors.
     * @return a pair of colored register on the left; or the register failed to be colored on the right.
     */
    std::pair <std::vector<size_t>, std::vector<size_t>> color(size_t colors);
};

#endif //GRAPH_COLORING_GRAPH_H
