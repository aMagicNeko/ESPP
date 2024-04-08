#pragma once
#include "pthread.h"

template <typename T> 
class Singleton {
public:
    static T* instance() {
        if(_s_obj_instance == NULL) {
            int ret = pthread_mutex_lock(&_s_mutex);
            if (0 != ret) {
                LOG(FATAL) << "bthread_mutex_lock errno=" << ret << " lock failed.";
                abort();
            }
            if(_s_obj_instance == NULL) {
                _s_obj_instance = new T;
            }
        }
        assert(_s_obj_instance != NULL);
        return _s_obj_instance;
    }
protected:
    Singleton () {}
    virtual ~Singleton() {}
private:
    Singleton(const Singleton &);
    Singleton & operator = (const Singleton &);
    static pthread_mutex_t _s_mutex;
    static T* _s_obj_instance;
};

template <typename T> T* Singleton<T>::_s_obj_instance = NULL;
template <typename T> pthread_mutex_t Singleton<T>::_s_mutex = PTHREAD_MUTEX_INITIALIZER;