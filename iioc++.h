#include "iio.h"
#include <complex>
#include <vector>
#include <string>
#include <assert.h>

class Buffer;
class Device;
class Context;
class Context_Devices;

class Buffer {
    iio_buffer* a;
    std::vector<std::complex<int16_t>> v;
public:
    ptrdiff_t step() const{
        return iio_buffer_step(a);
    }

    Buffer(const struct iio_device *dev, size_t samples_count = 1024*1024, bool cyclic = false) {
        a = iio_device_create_buffer(dev, samples_count, cyclic);
        int i = 0;
        for (auto t_dat = (char *)iio_buffer_first(a, NULL); t_dat < iio_buffer_end(a); t_dat += iio_buffer_step(a)) {
		    v.push_back(std::complex<int16_t>());
			v[i] = std::complex<int16_t>(((int16_t*)t_dat)[1], ((int16_t*)t_dat)[0]);
			i++;
		}
		assert(this->step() == sizeof(int16_t));
    }

    void destroy() {
        v.clear();
        iio_buffer_destroy(a);
    }

    ~Buffer() {
        this->destroy();
    }

    std::vector<std::complex<int16_t>>::iterator begin() {
        return v.begin();
    }

    std::vector<std::complex<int16_t>>::iterator end() {
        return v.end();
    }

    std::vector<std::complex<int16_t>>::const_iterator begin() const {
        return v.begin();
    }

    std::vector<std::complex<int16_t>>::const_iterator end() const {
        return v.end();
    }

    ssize_t push(size_t samples_count = 0) {
        int i = 0;
        for (auto t_dat = (char *)iio_buffer_first(a, NULL); t_dat < iio_buffer_end(a); t_dat += iio_buffer_step(a)) {
			((int16_t*)t_dat)[0] = v[i].imag();
			((int16_t*)t_dat)[1] = v[i].real();
			i++;
		}
        if (samples_count == 0) {
            return iio_buffer_push(a);
        } else {
            return iio_buffer_push_partial(a, samples_count);
        }
    }

    ssize_t refill() {
        auto ret = iio_buffer_refill(a);
        int i = 0;
        for (auto t_dat = (char *)iio_buffer_first(a, NULL); t_dat < iio_buffer_end(a); t_dat += iio_buffer_step(a)) {
			v[i] = std::complex<int16_t>(((int16_t*)t_dat)[1], ((int16_t*)t_dat)[0]);
			i++;
		}
        return ret;
    }
};

class Device {
    iio_device *dev;
public:
    Device(iio_device* device) {
        dev = device;
    }
    std::string id();
    std::string name();
};

class Context_Devices {
    Context* a;
public:
    Context_Devices(Context* b) {
        a = b;
    }
    int size();
    Device operator[] (unsigned int i);
    Device operator[] (std::string s);
};

class Context {
    iio_context* a;
    Context_Devices c;
public:
    friend Context_Devices;
    Context(iio_context* con): c(this) {
        a = con;
    }

    Context(): c(this) {
        a = iio_create_default_context();
    }

    Context(std::string type, std::string s = ""): c(this) {
        if (type == "uri") {
            a = iio_create_context_from_uri(s.c_str());
        } else if (type == "local") {
            a = iio_create_local_context();
        } else if (type == "network") {
            a = iio_create_network_context(s.c_str());
        } else {
            assert(0);
        }
    }

    Context(const Context& c): c(this) {
        a = iio_context_clone(c.a);
    }

    void destroy() {
        iio_context_destroy(a);
    }

    ~Context() {
        this->destroy();
    }

    Device find_device(std::string s) {
        return Device(iio_context_find_device(a, s.c_str()));
    }

    unsigned int devices_count() {
        return iio_context_get_devices_count(a);
    }
    std::string name() {
        return std::string(iio_context_get_name(a));
    }
};

std::string Device::id() {
    return std::string(iio_device_get_id(dev));
}

std::string Device::name() {
    return std::string(iio_device_get_name(dev));
}

int Context_Devices::size() {
    return iio_context_get_devices_count(a->a);
}

Device Context_Devices::operator[] (unsigned int i) {
    return iio_context_get_device(a->a, i);
}

Device Context_Devices::operator[] (std::string s) {
    return iio_context_find_device(a->a, s.c_str());
}
