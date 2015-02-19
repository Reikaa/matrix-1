/*******************************************************************
 ** tsemfifo.h | Template class for a Semaphore FIFO.
 *
 *  Copyright (C) 1995, 2004 Associated Universities, Inc. Washington DC,
 *  USA.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Correspondence concerning GBT software should be addressed as follows:
 *  GBT Operations
 *  National Radio Astronomy Observatory
 *  P. O. Box 2
 *  Green Bank, WV 24944-0002 USA
 *
 *******************************************************************/

#if !defined(_TSEMFIFO_H_)
#define _TSEMFIFO_H_

#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "TCondition.h"
#include "Mutex.h"
#include "ThreadLock.h"

/****************************************************************//**
 * \class tsemfifo
 *
 * Template class for a Semaphore FIFO.  This class features both
 * blocking and non-blocking put() and get() member functions.
 * The object being placed into the semaphore queue must have the same
 * copy semantics used in STL containers.
 *
 * An example use would be a callback that must not block posting to
 * this queue, and another thread blocking on the head of the queue
 * waiting for the events:
 *
 * Assuming a declaration somewhere like the following:
 *
 *     tsemfifo<int> fifo(10);  // 10 elements in queue
 *
 * In the posting thread:
 *
 *     if (fifo.try_put(data))  // Puts 'data' in queue
 *     {
 *        ... // data posted OK
 *     }
 *     else
 *     {
 *        ... // data could not be posted, queue full.
 *     }
 *
 * In handling thread:
 *
 *     int data;
 *
 *     fifo.get(data);          // blocks until 'data' arrives
 *
 *  For a post that blocks, use `put()` instead of `try_put()`, and for
 *  a get that doesn't block use `try_get()` instead of `get()`.
 *
 *******************************************************************/

template <typename T> class tsemfifo

{
  public:

    class Exception
    {
      public:

        enum
        {
            MSGLEN = 300
        };

        void what(int ec, char const *msg = 0);
        char const *what() const {return _what;}
        int error_code() const {return _err_code;}

      protected:

        int _err_code;
        char _what[MSGLEN + 1];
    };

    enum
    {
        FIFO_SIZE = 100,
    };

    tsemfifo(int size = FIFO_SIZE);
    ~tsemfifo();

    void release();
    void flush();
    bool put(T &obj);
    bool try_put(T &obj);
    bool get(T &obj);
    bool try_get(T &obj);
    bool wait_for_empty(int milliseconds = -1);
    unsigned int size();
    unsigned int capacity();

  private:

    tsemfifo(const tsemfifo &);
    tsemfifo &operator=(tsemfifo const &);

    void _create_sem();
    void _close_sem();
    void _get(T &obj);
    void _put(T &obj);

    T *_buffer;
    unsigned int _head;
    unsigned int _tail;
    unsigned int _buf_len;
    unsigned int _objects;
    sem_t _full_sem;
    sem_t _empty_sem;
    TCondition<bool> _release;
    TCondition<bool> _empty;

    Mutex _critical_section;
};

/****************************************************************//**
 * tsemfifo<T>::Exception::what(int ec, char const *msg)
 *
 ** Sets the error number and error message, with an optional caller
 *  provided message.
 *
 * @param ec: The error code
 * @param msg: The optional message.  If provided, the
 *        'what' string will be <optional message:> strerror_r().
 *        Otherwise, 'what' will be what is provided by strerror_r().
 *
 *******************************************************************/

template <class T> void tsemfifo<T>::Exception::what(int ec, char const *msg)

{
    char err[128];
    memset(err, 0, 128);
    strerror_r(ec, err, 127);

    _err_code = ec;

    if (msg)
    {
        snprintf(_what, MSGLEN, "%s: %s", msg, err);
    }
    else
    {
        strncpy(_what, err, 127);
    }
}

/****************************************************************//**
 * Construct a tsemfifo.  Allows the caller to specify the buffer size,
 * whether the put() operation will block on full buffer, and whether
 * the get() operation will block on empty buffer.
 *
 * @param size The capacity of the semaphore queue. If this capacity is
 * reached, `put()` will block, or `try_put()` will return an error.
 *
 *******************************************************************/

template <class T> tsemfifo<T>::tsemfifo(int size)
    : _buf_len(size)
    , _release(false),
      _empty(true)

{
    ThreadLock<Mutex> l(_critical_section);

    _buffer = new T[size];
    l.lock();
    _create_sem();
    _head = _tail = _objects = 0;
}

/****************************************************************//**
 * Destructor for tsemfifo FIFO class.  Releases memory, semaphores etc.
 *
 *******************************************************************/

template <class T> tsemfifo<T>::~tsemfifo()

{
    ThreadLock<Mutex> l(_critical_section);

    l.lock();
    _close_sem();
    delete [] _buffer;
}

/****************************************************************//**
 * This private member function creates the semaphores with the proper
 * counts.
 *
 *******************************************************************/

template <class T> void tsemfifo<T>::_create_sem()

{
    sem_init(&_full_sem, 0, 0);
    sem_init(&_empty_sem, 0, _buf_len);
}

/****************************************************************//**
 * This private member function destroys the semaphore objects.
 *
 *******************************************************************/

template <class T> void tsemfifo<T>::_close_sem()

{
    sem_destroy(&_full_sem);
    sem_destroy(&_empty_sem);
}

/****************************************************************//**
 * Empties the queue. Throws a tsemfifo<T>::Exception if there is a
   semaphore resource issue.
 *
 *******************************************************************/

template <class T> void tsemfifo<T>::flush()

{
    ThreadLock<Mutex> l(_critical_section);

    l.lock();
    _close_sem();
    _create_sem();

    _release.set_value(false);
    _empty.set_value(true);
    _head = _tail = _objects = 0;
}

/****************************************************************//**
 * Blocks until the FIFO is empty.  This is useful for another task to
 * wait until the FIFO is empty before doing something, like closing a
 * file handle, ending a thread, etc.
 *
 * @param milliseconds: The number of milliseconds to wait before
 *        abandoning the wait.
 *
 * @return true if wait succeeded, false if wait timed out.
 *
 *******************************************************************/

template <class T> bool tsemfifo<T>::wait_for_empty(int milliseconds)

{
    if (milliseconds == -1)
    {
        _empty.wait(true);
        return true;
    }

    return _empty.wait(true, milliseconds * 1000);
}

/****************************************************************//**
 * This private function actually does the work of updating the FIFO
 * with a new value, once the public put() or try_put() functions have
 * determined there is enough room for the value.
 *
 * @param obj: Object to put (copy) into the buffer.
 *
 *******************************************************************/

template <class T> void tsemfifo<T>::_put(T &obj)

{
    ThreadLock<Mutex> l(_critical_section);


    l.lock();
    _buffer[_tail] = obj;

    if (_tail < (_buf_len - 1))
    {
        ++_tail;
    }
    else
    {
        _tail = 0;
    }

    if (!_objects)                   // Was empty, now has something.
    {                                // clear the empty condition variable/event
        _empty.set_value(false);
    }

    ++_objects;
    l.unlock();

    if (sem_post(&_full_sem) == -1)
    {
        Exception e;
        e.what(errno, "tsemfifo<T>::_put()");
        throw e;
    }
}

/****************************************************************//**
 * Puts a new value at the tail of the FIFO.  put() will block if the
 * buffer is full. Throws an exception if there is a sem_wait() problem.
 *
 * @param obj: Object to put (copy) into the buffer.
 *
 * @return true if the put succeeds, false otherwise.
 *
 *******************************************************************/

template <class T> bool tsemfifo<T>::put(T &obj)

{
    int r;

    do
    {
        r = sem_wait(&_empty_sem);

        if (r == -1 && errno != EINTR)
        {
            Exception e;
            e.what(errno, "tsemfifo<T>::put()");
            throw e;
        }
    }
    while (r == -1 && errno != EDEADLK);

    if (_release.wait(true, 0))
    {
        return false;
    }

    _put(obj);
    return true;
}

/****************************************************************//**
 * Puts a new value at the tail of the FIFO.  try_put() will not block
 * if the buffer is full.  Instead, it immediately returns false without
 * placing anything in the queue.
 *
 * Throws a tsemfifo::Exception if there is a sem_trywait() failure.
 *
 * @param obj: Object to put (copy) into the buffer.
 *
 * @return true on success, false otherwise. A return value of false
 * indicates that the queue is full, and if `put()` rather than
 * `try_put()` had been used it would have blocked.
 *
 *******************************************************************/

template <class T> bool tsemfifo<T>::try_put(T &obj)

{
    if (sem_trywait(&_empty_sem) == -1)
    {
        Exception e;
        e.what(errno, "tsemfifo<T>::try_put()");

        if (e.error_code() == EAGAIN)
        {
            return false;
        }

        throw e;
    }

    _put(obj);
    return true;
}

/****************************************************************//**
 * This private helper function actually does the manipulatio of the
 * FIFO to retrieve an object for get() and try_get() once these have
 * determined that there is an object to get.
 *
 *  @param obj: object to which FIFO object will be copied to.
 *
 *******************************************************************/

template <class T> void tsemfifo<T>::_get(T &obj)

{
    ThreadLock<Mutex> l(_critical_section);

    l.lock();
    obj = _buffer[_head];

    if (_head < (_buf_len - 1))
    {
        ++_head;
    }
    else
    {
        _head = 0;
    }

    --_objects;

    l.unlock();

    if (!_objects)               // Was not empty, now empty.  Set empty event.
    {
        _empty.broadcast(true);
    }

    if (sem_post(&_empty_sem) == -1)
    {
        Exception e;
        e.what(errno, "tsemfifo<T>::_get()");
        throw e;
    }
}

/****************************************************************//**
 * Gets a value out of the head of the FIFO.  get() will block,
 * suspending the calling thread, until something gets placed into the
 * FIFO.
 *
 * @param obj: object to which FIFO object will be copied to.
 *
 * @return true if get() succeeded, false if get() was blocked and
 *         released.
 *
 *******************************************************************/

template <class T> bool tsemfifo<T>::get(T &obj)

{
    int r;

    do
    {
        r = sem_wait(&_full_sem);

        if (r == -1 && errno != EINTR)
        {
            Exception e;
            e.what(errno, "tsemfifo<T>::get()");
            throw e;
        }
    }
    while (r == -1 && errno != EDEADLK);

    if (_release.wait(true, 0))
    {
        return false;
    }

    _get(obj);
    return true;
}

/****************************************************************//**
 * Gets a value out of the head of the FIFO.  try_get() will not block
 * if there is nothing at the head of the FIFO.  See return value.
 *
 * @param obj: object to which FIFO object will be copied to.
 *
 * @return try_get() will return true if there was a value at the head
 *         of the FIFO, false if the FIFO was empty.
 *
 *******************************************************************/

template <class T> bool tsemfifo<T>::try_get(T &obj)

{
    if (sem_trywait(&_full_sem) == -1)
    {
        Exception e;
        e.what(errno, "tsemfifo<T>::try_get()");

        if (e.error_code() == EAGAIN)
        {
            return false;
        }

        throw e;
    }

    _get(obj);
    return true;
}

/****************************************************************//**
 * If any thread is waiting on get() or put(), this will release them.
 * The queue should not be used after this call unless the next call is
 * flush().
 *
 *******************************************************************/

template <class T> void tsemfifo<T>::release()

{
    _release.broadcast(true);
    sem_post(&_full_sem);
    sem_post(&_empty_sem);
}

/****************************************************************//**
 * Returns the number of objects in the FIFO.
 *
 * @return The number of objects in the FIFO.
 *
 *******************************************************************/

template <class T> unsigned int tsemfifo<T>::size()

{
  unsigned int o;
  ThreadLock<Mutex> l(_critical_section);


  l.lock();
  o = _objects;
  return o;

}

/****************************************************************//**
 * Returns the maximum size of the FIFO, in objects of type T.
 *
   @return The maximum number of objects that the FIFO can hold.
 *
 *******************************************************************/

template <class T> unsigned int tsemfifo<T>::capacity()

{
  unsigned int o;
  ThreadLock<Mutex> l(_critical_section);


  l.lock();
  o = _buf_len;
  return o;

}

#endif  // _TSEMFIFO_H_