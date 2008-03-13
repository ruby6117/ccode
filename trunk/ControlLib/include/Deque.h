#ifndef Deque_H
#define Deque_H


/**
   @brief A Real-time-safe Double-ended queue in the spirit of std::deque.

   A class that emulates STL's Deque, but it is realtime-safe since it
   uses a buffer pre-allocated at construction time (which presumably happens
   in non-realtime context) */
template <typename T>
class Deque
{
  typedef long pos_type;

public:
  typedef T & reference;
  typedef const T & const_reference;
  typedef unsigned long size_type;

  Deque(size_type max_capacity = 1024)
  {
    if (!max_capacity) max_capacity = 1;
    max_size = max_capacity; head_idx = 0;  sz = 0;
    cb = new T[max_size];
  }

  Deque(const Deque & other) : cb(0) { *this = other; }

  Deque & operator=(const Deque & other) {
    if (cb) delete [] cb;
    max_size = other.max_size;
    sz = other.sz;
    head_idx = other.head_idx;
    cb = new T[max_size];
    for (pos_type i = 0; i < sz; ++i) {
      pos_type idx = (head_idx + i) % max_size;
      cb[idx] = other.cb[idx];
    }
    return *this;
  }

  ~Deque() { delete [] cb; }
  
  reference front() { return cb[head_idx]; }
  const_reference front() const { return cb[head_idx]; }
  reference back() { return cb[tail()]; }
  const_reference back() const { return cb[tail()]; }
  
  size_type capacity() const { return max_size; }

  size_type size() const { return sz; }
    
  void pop_front() { 
    if (sz == 0) return;
    --sz;
    if (++head_idx >= max_size) head_idx -= max_size;    
  }

  void pop_back() { 
    if (sz == 0) return;
    --sz;
  }

  void push_front(const_reference t) {
    if (size() >= capacity()) return;
    --head_idx;
    ++sz;
    if (head_idx < 0) head_idx += max_size;
    cb[head_idx] = t;
  }

  void push_back(const_reference t) {
    if (size() >= capacity()) return;
    ++sz;
    cb[tail()] = t;
  }

  void clear() { sz = 0; head_idx = 0; }

private:
  pos_type max_size, sz;
  pos_type head_idx;
  T *cb;

  size_type tail() const { return (head_idx + sz - 1) % max_size; }
  
};

#ifdef TEST_DEQUE

#include <iostream>
using namespace std;

int main(void)
{
  Deque<double> d1, d2(5);

  d1.push_back(1.1);
  d1.push_back(1.2);
  d1.push_back(1.3);
  d1.push_back(1.4);
  d1.push_back(1.5);
  d1.push_back(1.6);
  d1.push_back(1.7);
  d1.push_back(1.8);
  d1.pop_front();
  d1.push_front(1.09);
  d1.pop_back();
  d1.push_back(1.89);
  while (d1.size()) {
    double v = d1.front();
    d1.pop_front();
    cout << v << "\n";
  }

  d2.push_back(1.1);
  d2.push_back(1.2);
  d2.push_back(1.3);
  d2.push_back(1.4);
  d2.push_back(1.5);
  d2.push_back(1.6);
  d2.push_back(1.7);
  d2.push_back(1.8);
  d2.pop_back();
  d2.pop_front();
  d2.push_back(99.9);
  d2.push_back(9999.9);
  d2.pop_front();
  d2.push_front(101.1);
  Deque<double> d3(d2);
  while (d3.size()) {
    double v = d3.back();
    d3.pop_back();
    cout << v << "\n";
  }
  
  return 0;
}
#endif

#endif
