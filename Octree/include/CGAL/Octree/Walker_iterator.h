
#ifndef OCTREE_WALKER_ITERATOR_H
#define OCTREE_WALKER_ITERATOR_H

#include <boost/iterator/iterator_facade.hpp>

namespace CGAL {

  template<class Value>
  class Walker_iterator :
          public boost::iterator_facade<Walker_iterator<Value>, Value, boost::forward_traversal_tag> {

  public:

    Walker_iterator() : m_value(nullptr) {}

    Walker_iterator(Value *value) : m_value(value) {}

  private:
    friend class boost::iterator_core_access;

    bool equal(Walker_iterator<Value> const &other) const {
      return m_value == other.m_value;
    }

    void increment() {
      // TODO
    }

    Value &dereference() const {
      return m_value;
    }

  private:

    Value *m_value;
  };

}

#endif //OCTREE_WALKER_ITERATOR_H
