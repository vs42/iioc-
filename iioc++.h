#include "iio.h"

class Buffer;

class Iterator_Buffer {
protected:
    Buffer* buf;
    char* a;
    ptrdiff_t step;
public:
    Iterator_Buffer(const Iterator_Buffer &x) {
        buf = x.buf;
        a = x.a;
        step = x.step;
    }

    Iterator_Buffer(Buffer* b, char* c, ptrdiff_t s) {
        buf = b;
        a = c;
        step = s;
    }

// Define prefix increment operator.
    Iterator_Buffer& operator++()
    {
       a += step;
       return *this;
    }

// Define postfix increment operator.
    Iterator_Buffer operator++(int)
    {
       Iterator_Buffer temp = *this;
       ++*this;
       return temp;
    }

    int16_t* operator*() {
       return (int16_t*)a;
    }

    bool operator==(const Iterator_Buffer& a) const {
        return (a.buf == this->buf) && (a.a == this->a);
    }
    bool operator!=(const Iterator_Buffer& a) const {
        return !((*this) == a);
    }
};

class constIterator_Buffer {
protected:
    const Buffer* buf;
    char* a;
    ptrdiff_t step;
public:
    constIterator_Buffer(constIterator_Buffer &x) {
        buf = x.buf;
        a = x.a;
        step = x.step;
    }

    constIterator_Buffer(const Buffer* b, char* c, ptrdiff_t s) {
        buf = b;
        a = c;
        step = s;
    }

// Define prefix increment operator.
    constIterator_Buffer& operator++()
    {
       a += step;
       return *this;
    }

// Define postfix increment operator.
    constIterator_Buffer operator++(int)
    {
       constIterator_Buffer temp = *this;
       ++*this;
       return temp;
    }

    const int16_t* operator*() {
       return (int16_t*)a;
    }
};


class Buffer {
    iio_buffer *a;
public:
    ptrdiff_t step() const{
        return iio_buffer_step(a);
    }
    Buffer(const struct iio_device *dev, size_t samples_count = 1024*1024, bool cyclic = false) {
        a = iio_device_create_buffer(dev, samples_count, cyclic);
    }
    void destroy() {
        iio_buffer_destroy(a);
    }
    ~Buffer() {
        this->destroy();
    }
    Iterator_Buffer begin() {
        iio_buffer_first(a, NULL);
        Iterator_Buffer it(this, (char *)iio_buffer_first(a, NULL), iio_buffer_step(a));
        return it;
    }
    Iterator_Buffer end() {
        iio_buffer_first(a, NULL);
        Iterator_Buffer it(this, (char *)iio_buffer_end(a), iio_buffer_step(a));
        return it;
    }
    constIterator_Buffer begin() const {
        iio_buffer_first(a, NULL);
        constIterator_Buffer it(this, (char *)iio_buffer_first(a, NULL), iio_buffer_step(a));
        return it;
    }
    constIterator_Buffer end() const {
        iio_buffer_first(a, NULL);
        constIterator_Buffer it(this, (char *)iio_buffer_end(a), iio_buffer_step(a));
        return it;
    }
    ssize_t push(size_t samples_count = 0) {
        if (samples_count == 0) {
            return iio_buffer_push(a);
        } else {
            return iio_buffer_push_partial(a, samples_count);
        }
    }
    ssize_t refill() {
        return iio_buffer_refill(a);
    }
    int16_t* operator[](size_t x) {
        return (int16_t*)((char *)iio_buffer_first(a, NULL) + (iio_buffer_step(a) * x));
    }
};
