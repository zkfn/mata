//
// Created by Lukáš Holík on 06.12.2022.
//

#ifndef LIBMATA_NUMBER_PREDICATE_HH
#define LIBMATA_NUMBER_PREDICATE_HH

#include <vector>

#include "ord-vector.hh"

namespace Mata::Util {

template <class Number> class OrdVector;

/**
 * @brief An enhanced boolean array, implementing a set of numbers, aka a unary predicate over numbers, that provides a
 *  constant test and update.
 *
 * A number that is explicitly added is in the set, all the other numbers are implicitly not in the set.
 *
 * Besides a vector of bools (predicate), it can also be asked to maintain a vector of elements (elements).
 * To keep constant test and set, new elements are pushed back to the vector but remove does not modify the vector.
 * Hence, after a remove, the vector contains a superset of the true elements.
 * Superset is still useful, to iterate through true elements, iterate through the vector and test membership in the bool array.
 * tracking_elements indicates that elements are tracked in the vector of elements as described above.
 * elements_are_exact indicates that the vector of elements contains the exact set of true elements.
 * INVARIANT:
 *  tracking_elements -> elements contains a superset of the true elements
 *  elements_are_exact -> elements contain exactly the true elements
 *
 * predicate.size() is referred to as "the size of the domain". Ideally, the "domain" would not be visible form the outside,
 * but the size of the domain is going to be used to determine the number of states in the nfa automaton :(.
 * This is somewhat ugly, but don't know how else to do it efficiently (computing true maximum would be costly).
 * TODO: Rewrite and simplify this monstrosity! So far, it is not clear that remembering elements gains any speed (seem to cost more than it saves), so lets use a simple on which does not.
 *  We can also implement another one, which does remember elements, and we can have move constructors to switch between them. It will be much simpler code.
 */
template<typename Number> class NumberPredicate {
private:
    mutable std::vector<bool> predicate = {};
    mutable std::vector<Number> elements = {};
    mutable bool elements_are_exact = true;
    mutable bool tracking_elements = true;
    mutable Number cardinality = 0;

    using const_iterator = typename std::vector<Number>::const_iterator;

    /**
     * assuming that elements contain a superset of the true elements, prune them (in situ)
     */
    void prune_elements() const {
        Number new_pos = 0;
        for (Number orig_pos = 0; orig_pos < elements.size(); ++orig_pos) {
            if (predicate[elements[orig_pos]]) {
                elements[new_pos] = elements[orig_pos];
                ++new_pos;
            }
        }
        elements.resize(new_pos);
        //there was a bug here! That line below was missing. Maybe not efficient.
        //Well, it was maybe not a bug, not sure. Reverting, but keeping this comments here.
        //Util::sort_and_rmdupl(elements);
        elements_are_exact = true;
    }

    /**
     * assuming nothing, compute the true elements from scratch
     */
    void compute_elements() const {
        elements.clear();
        elements.reserve(cardinality);
        for (Number q = 0, size=predicate.size(); q < size; ++q) {
            if (predicate[q])
                elements.push_back(q);
        }
        elements_are_exact = true;
    }

    /**
     * calls prune_elements or compute_elements based on the state of the indicator variables
     */
    void update_elements() const {
        if (!elements_are_exact) {
            if (!tracking_elements) {
                compute_elements();
            } else {
                prune_elements();
            }
        }
    }

public:
    NumberPredicate(Number size, bool val, bool track_elements = true)
        : predicate(size, val), elements_are_exact(true), tracking_elements(track_elements) {
        if (tracking_elements && val) {
            elements.reserve(size);
            for (Number e = 0;e<size;++e)
                elements.push_back(e);
        }
        if (!val)
            cardinality = 0;
        else
            cardinality = size;
    }

    NumberPredicate() = default;
    explicit NumberPredicate(const bool track_elements) : tracking_elements(track_elements) {}
    NumberPredicate(std::initializer_list<Number> list, bool track_elements = true)
        : tracking_elements(track_elements) {
        for (const auto& q: list) { add(q); }
    }

    explicit NumberPredicate(const std::vector<Number>& list, const bool track_elements = true)
        : tracking_elements(track_elements) { add(list); }

    explicit NumberPredicate(const std::vector<bool>& bv, bool track_elements = true)
        : tracking_elements(track_elements) {
        predicate.reserve(bv.size());
        for (size_t i = 0;i<bv.size();i++) {
            if (bv[i]) { add(i); }
        }
    }

    explicit NumberPredicate(const Mata::Util::OrdVector<Number>& vec, bool track_elements = true)
        : tracking_elements(track_elements) {
        for (const auto& q: vec) { add(q); }
    }

    NumberPredicate(const NumberPredicate<Number>& rhs) = default;
    NumberPredicate(NumberPredicate<Number>&& other) noexcept
        : predicate{ std::move(other.predicate) }, elements{ std::move(other.elements) },
          elements_are_exact{ other.elements_are_exact}, tracking_elements{ other.tracking_elements },
          cardinality{ other.cardinality} {
        other.cardinality = 0;
    }

    NumberPredicate<Number>& operator=(const NumberPredicate<Number>& rhs) = default;
    NumberPredicate<Number>& operator=(NumberPredicate<Number>&& other) noexcept {
        if (this != &other) {
            elements = std::move(other.elements);
            predicate = std::move(other.predicate);
            elements_are_exact = other.elements_are_exact;
            tracking_elements = other.tracking_elements;
            cardinality = other.cardinality;
            other.cardinality = 0;
        }
        return *this;
    }

    template <class InputIterator>
    NumberPredicate(InputIterator first, InputIterator last)
    {
        while (first < last) {
            add(*first);
            ++first;
        }
    }

    /*
     * Note that it extends predicate if q is out of its current domain.
     */
    void add(Number q) {
        if (!(*this)[q]) {
            cardinality++;
            if (predicate.size() <= q) {
                reserve_on_insert(predicate, q);
                predicate.resize(q + 1, false);
            }
            if (tracking_elements) {
                Number q_was_there = predicate[q];
                predicate[q] = true;
                if (!q_was_there) {
                    reserve_on_insert(elements);
                    elements.push_back(q);
                }
            } else {
                predicate[q] = true;
                elements_are_exact = false;
            }
        }
    }

    void remove(Number q) {
        if ((*this)[q]) {
            cardinality--;
            elements_are_exact = false;
            if (q < predicate.size() && predicate[q]) {
                predicate[q] = false;
            }
        }
    }

    void add(const std::vector <Number> &elems) {
        for (Number q: elems)
            add(q);
    }

    void remove(const std::vector <Number> &elems) {
        for (Number q: elems)
            remove(q);
    }

    /**
     * start tracking elements (might require updating them to establish the invariant)
     */
    void track_elements() {
        if (!tracking_elements) {
            update_elements();
            tracking_elements = true;
        }
    }

    void dont_track_elements() {
        tracking_elements = false;
    }

    // Defragmentation:
    // is_staying[q] == true if q is to stay in the domain, else it is to be removed from the domain,
    // and the names of all r > q in the domain will get decremented.
    // So far, I will make it efficient only for the case when elements are not tracked.
    void defragment(const NumberPredicate<Number> & is_staying) {
        Number max_positive = 0;//new maximum positive element of the domain
        Number new_dom_max = 0;
        cardinality = 0;
        for (Number i_old=0;i_old<predicate.size() && i_old < is_staying.domain_size(); ++i_old) {
            if (is_staying[i_old]) {
                predicate[new_dom_max] = predicate[i_old];
                if (predicate[i_old]) {
                    cardinality++;
                    max_positive = new_dom_max;
                }
                new_dom_max++;
            }
            else
                elements_are_exact = false;
        }
        if (cardinality > 0)
            predicate.resize(max_positive+1);
        else
            predicate.resize(0);
        if (tracking_elements) {
            compute_elements();
        }
    }

    /**
     * @return True if predicate for @p q is set. False otherwise (even if q is out of range of the predicate).
     */
    bool operator[](Number q) const {
        if (q < predicate.size())
            return predicate[q];
        else
            return false;
    }

    /**
     * This is the number of the true elements, not the size of any data structure.
     */
    Number size() const {
        return cardinality;
        //if (elements_are_exact)
        //    return elements.size();
        //if (tracking_elements) {
        //    prune_elements();
        //    return elements.size();
        //} else {
        //    Number cnt = 0;
        //    for (Number q = 0, size = predicate.size(); q < size; ++q) {
        //        if (predicate[q])
        //            cnt++;
        //    }
        //    return cnt;
        //}
    }

    /*
     * Clears the set of true elements. Does not clear the predicate, only sets it false everywhere.
     */
    void clear() {
        if (tracking_elements)
            for (size_t i = 0, size = elements.size(); i < size; i++)
                predicate[elements[i]] = false;
        else
            //for (size_t i = 0, size = predicate.size(); i < size; i++)
            //    predicate[i] = false;
            // Vysvetlete mi, co dela to && nize a zaplatim obed.
            for (auto && i : predicate)
                i = false;
        elements.clear();
        elements_are_exact = true;
        cardinality = 0;
    }

    void reserve(Number n) {
        predicate.reserve(n);
        if (tracking_elements) {
            elements.reserve(n);
        }
    }

    //TODO: or negate()?
    void flip(Number q) {
        if ((*this)[q])
            remove(q);
        else
            add(q);
    }

    /*
     * Complements the set with respect to a given number of elements = the maximum number + 1.
     * Should be somewhat efficient.
     */
    void complement(Number domain_size) {
        Number old_domain_size = predicate.size();
        predicate.reserve(domain_size);
        Number to_flip = std::min(domain_size,old_domain_size);
        for (Number q = 0; q < to_flip; ++q) {
            //predicate[q] = !predicate[q];
            flip(q);
        }
        if (domain_size > old_domain_size)
            for (Number q = old_domain_size; q < domain_size; ++q) {
                predicate.push_back(true);
                cardinality++;
            }
        else if (domain_size < old_domain_size)
            for (Number q = domain_size; q < old_domain_size; ++q) {
                if (predicate[q])
                    cardinality--;
                predicate[q]=false;
            }
        if (tracking_elements) {
            compute_elements();
        }
    }

    /*
     * Iterators to iterate through the true elements. No order can be assumed.
     * This implementation is lazy. For instance, when not tracking elements, we should not compute elements.
     */
    inline const_iterator begin() const {
        update_elements();
        return elements.begin();
    }

    inline const_iterator end() const {
        update_elements();
        return elements.end();
    }

    inline const_iterator cbegin() const {
        update_elements();
        return elements.cbegin();
    }

    inline const_iterator cend() const {
        update_elements();
        return elements.cend();
    }

    inline const_iterator begin() {
        update_elements();
        return elements.begin();
    }

    inline const_iterator end() {
        update_elements();
        return elements.end();
    }

    const std::vector <Number> &get_elements() const {
        update_elements();
        return elements;
    }

    bool are_disjoint(const NumberPredicate<Number> &other) const {
        //return std::all_of(begin(),end(),[&other](Number x) {return !other[x];});//Vau!
        for (auto q: *this)
            if (other[q])
                return false;
        return true;
    }

    bool empty() const {
        return (size() == 0);
    }

    // This is supposed to return something not smaller than the largest element in the set
    // the easiest is to return the size of the predicate, roughly, the largest element ever inserted.
    Number domain_size() const {
        return predicate.size();
    }

    // truncates the domain to the maximal element (so the elements stay the same)
    // this could be needed in defragmentation of nfa
    void truncate_domain() {
        if (predicate.empty())
            return;
        else if (predicate[predicate.size() - 1])
            return;

        Number max;
        if (tracking_elements) {
            update_elements();
            // if empty, max would be something strange
            if (elements.empty()) {
                predicate.resize(0);
                return;
            }
            else
                max = *std::max_element(elements.begin(), elements.end());
        }
        else {
            // one needs to be careful, Number can be unsigned, empty predicate would cause 0-1 below
            for (max = predicate.size() - 1; max >= 0; --max)
                if (predicate[max])
                    break;
        }

        predicate.resize(max + 1);
    }

    /**
     * Renames numbers in predicate according to the given @renaming. If a number is not present in @renaming
     * it is renamed to base + offset value. Rationalization for this is that base should e.g.,
     * higher than number of states in Nfa delta so we rename the initial or final states not presented in delta
     * to numbers just after delta. Offset is then increased after encountering each of such states
     * not presented in delta.
     * TODO: used only in defragment of Nfa, which is not used anywhere. Remove?
     */
    void rename(const std::vector<Number>& renaming, const Number base = 0) {
        if (renaming.empty())
            return; // nothing to rename

        Number offset = 0;
        std::vector<Number> single_states_renaming;

        update_elements();

        auto max_or_default = [](const std::vector<Number>& container, Number def) {
            auto max_it = std::max_element(container.begin(), container.end());
            return (max_it == container.end() ? def : *max_it);
        };

        std::vector<Number> new_elements;
        std::vector<bool> new_predicate(std::max(max_or_default(elements, 0) + 1, max_or_default(renaming, 0) + 1));

        for (const Number& number : elements) {
            if (number < renaming.size()) { // number is renamed by provided vector
                new_elements.push_back(renaming[number]);
                if (predicate[number])
                    new_predicate[renaming[number]] = true;
            }
            else { // number not defined in provided vector, needs to be renamed by this function
                if (number >= single_states_renaming.size()) {
                    single_states_renaming.resize(number + 1);
                    single_states_renaming[number] = base + offset;
                    ++offset;
                }

                new_elements.push_back(single_states_renaming[number]);
                if (predicate[number])
                    new_predicate[single_states_renaming[number]] = true;
            }
        }

        elements = new_elements;
        predicate = new_predicate;
    }
};

template <typename Number>
bool are_disjoint(Mata::Util::NumberPredicate<Number> lhs, Mata::Util::NumberPredicate<Number> rhs) {
    return lhs.are_disjoint(rhs);
}

} // Namespace Mata::Util.

#endif // LIBMATA_NUMBER_PREDICATE_HH