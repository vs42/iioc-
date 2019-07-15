#include "iio.h"
#include <complex>
#include <vector>
#include <string>
#include <cassert>
#include <cerrno>

const int MAXATRLENGTH = 128;
namespace Hz{
    long long operator"" _MHz(unsigned long long x) {
        return ((long long)x*1000000.0 + .5);
    }

    long long operator"" _GHz(unsigned long long x) {
        return ((long long)x*1000000000.0 + .5);
    }

    long long operator"" _MHz(long double x) {
        return (x*1000000.0 + .5);
    }

    long long operator"" _GHz(long double x) {
        return (x*1000000000.0 + .5);
    }
}

class Buffer;
class Device;
class Context;
class Channel;
class Device_Channels;
class Device_Attribute;
class Channel_Attribute;
class Device_Attributes;
class Channel_Attributes;
class Context_Devices;

class Device_Attribute {
    Device *dev;
public:
    std::string key;
    Device_Attribute (std::string str, Device *device) {
        key = str;
        dev = device;
    }
    Device_Attribute& operator =(std::string const& str);
    Device_Attribute& operator =(const char* str);
    Device_Attribute& operator =(long long str);
    Device_Attribute& operator =(double str);
    Device_Attribute& operator =(bool str);
    std::string value();
};

class Device_Attributes {
    Device* a;
public:
    Device_Attributes(Device* b) {
        a = b;
    }
    int size();
    std::string operator[] (unsigned int i);
    Device_Attribute operator[] (std::string s);
};

class Device_Channels {
    Device* a;
    bool out;
public:
    Device_Channels(Device* b, bool outc) {
        a = b;
        out = outc;
    }
    int size();
    Channel operator[] (std::string s);
};

class Device {
    iio_device *dev;
public:
    friend Buffer;
    friend Device_Attributes;
    friend Device_Attribute;
    friend Device_Channels;
    Device_Channels in;
    Device_Channels out;
    Device_Attributes attributes;
    Device(iio_device* device) : attributes(this), in(this, false), out(this, true) {
        dev = device;
    }
    size_t sample_size() {
        return iio_device_get_sample_size(dev);
    }
    std::string id();
    std::string name();
    Channel find_channel(std::string s, bool output);
};

class Buffer {
    iio_buffer* a;
    std::vector<std::complex<int16_t>> v;
public:
    ptrdiff_t step() const{
        return iio_buffer_step(a);
    }

    Buffer(Device dev, size_t samples_count = 1024*1024, bool cyclic = false) 
        : v(samples_count)
    {
        if ((a = iio_device_create_buffer(dev.dev, samples_count, cyclic)) == nullptr) {
            throw std::system_error{errno, std::generic_category(), "buffer not created"};
        }
        
        int i = 0;
        for (auto t_dat = (char *)iio_buffer_start(a); t_dat < iio_buffer_end(a); t_dat += iio_buffer_step(a)) {
		    //v.push_back(std::complex<int16_t>());
			v[i] = std::complex<int16_t>(((int16_t*)t_dat)[1], ((int16_t*)t_dat)[0]);
			i++;
		}
		assert(this->step() == sizeof(int16_t) * 2);
    }

    void destroy() {
        v.clear();
        iio_buffer_destroy(a);
    }

    void set_blocking_mode(bool x) {
        int err;
        if ((err = iio_buffer_set_blocking_mode(a, x)) < 0) {
            throw std::system_error{-err, std::generic_category(), "blocking mode changing error"};
        }
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
        for (auto t_dat = (char *)iio_buffer_start(a); t_dat < iio_buffer_end(a); t_dat += iio_buffer_step(a)) {
			((int16_t*)t_dat)[0] = v[i].imag();
			((int16_t*)t_dat)[1] = v[i].real();
			i++;
		}
        int err = 0;
        if (samples_count == 0) {
            return iio_buffer_push(a);
        } else {
            return iio_buffer_push_partial(a, samples_count);
        }
    }

    ssize_t refill() {
        auto ret = iio_buffer_refill(a);
        int i = 0;
        for (auto t_dat = (char *)iio_buffer_start(a); t_dat < iio_buffer_end(a); t_dat += iio_buffer_step(a)) {
			v[i] = std::complex<int16_t>(((int16_t*)t_dat)[1], ((int16_t*)t_dat)[0]);
			i++;
		}
        return ret;
    }
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
public:
    Context_Devices devices;
    friend Context_Devices;
    friend Device;
    Context(iio_context* con): devices(this) {
        a = con;
    }

    Context(): devices(this) {
        a = iio_create_default_context();
    }

    Context(std::string type, std::string s = ""): devices(this) {
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

    Context(const Context& c): devices(this) {
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

class Channel_Attribute {
    Channel *dev;
public:
    std::string key;
    Channel_Attribute (std::string str, Channel *device) {
        key = str;
        dev = device;
    }
    Channel_Attribute& operator =(std::string const& str);
    Channel_Attribute& operator =(const char* str);
    Channel_Attribute& operator =(long long str);
    Channel_Attribute& operator =(double str);
    Channel_Attribute& operator =(bool str);
    std::string value();
};

class Channel_Attributes {
    Channel* a;
public:
    Channel_Attributes(Channel* b) {
        a = b;
    }
    int size();
    std::string operator[] (unsigned int i);
    Channel_Attribute operator[] (std::string s);
};

class Channel {
    iio_channel *a;
public:
    friend Channel_Attributes;
    friend Channel_Attribute;
    Channel_Attributes attributes;

    Channel (iio_channel *b) : a(b), attributes(this) {
    }

    std::string name() {
        return std::string(iio_channel_get_name(a));
    }

    std::string id() {
        return std::string(iio_channel_get_id(a));
    }

    void enable() {
        iio_channel_enable(a);
    }

    void disable() {
        iio_channel_disable(a);
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
    return Device(iio_context_get_device(a->a, i));
}

Device Context_Devices::operator[] (std::string s) {
    return Device(iio_context_find_device(a->a, s.c_str()));
}

std::string Channel_Attributes::operator[] (unsigned int i) {
    return std::string(iio_channel_get_attr(a->a, i));
}

Channel_Attribute Channel_Attributes::operator[] (std::string s) {
    return Channel_Attribute(s, a);
}

std::string Device_Attributes::operator[] (unsigned int i) {
    return std::string(iio_device_get_attr(a->dev, i));
}

Device_Attribute Device_Attributes::operator[] (std::string s) {
    return Device_Attribute(s, a);
}

Channel Device::find_channel(std::string s, bool output) {
    return Channel(iio_device_find_channel(dev, s.c_str(), output));
}

Device_Attribute& Device_Attribute::operator =(std::string const& str) {
    int err;
    if((err = iio_device_attr_write(dev->dev, key.c_str(), str.c_str())) < 0) {
        throw std::system_error{-err, std::generic_category(), "device attribute write error"};
    }
    return *this;
}

Device_Attribute& Device_Attribute::operator =(const char* str) {
    int err;
    if((err = iio_device_attr_write(dev->dev, key.c_str(), str)) < 0) {
        throw std::system_error{-err, std::generic_category(), "device attribute write error"};
    }
    return *this;
}

Device_Attribute& Device_Attribute::operator = (long long str){
    int err;
    if((err = iio_device_attr_write_longlong(dev->dev, key.c_str(), str)) < 0) {
        throw std::system_error{-err, std::generic_category(), "device attribute write error"};
    }
    return *this;
}

Device_Attribute& Device_Attribute::operator = (bool str){
    int err;
    if ((err = iio_device_attr_write_bool(dev->dev, key.c_str(), str)) < 0) {
        throw std::system_error{-err, std::generic_category(), "device attribute write error"};
    }
    return *this;
}

Device_Attribute& Device_Attribute::operator = (double str){
    int err;
    if ((err = iio_device_attr_write_double(dev->dev, key.c_str(), str)) < 0) {
        throw std::system_error{-err, std::generic_category(), "device attribute write error"};
    }
    return *this;
}

std::string Device_Attribute::value() {
    char tmp[MAXATRLENGTH];
    iio_device_attr_read(dev->dev, key.c_str(), tmp, MAXATRLENGTH);
    return std::string(tmp);
}

Channel_Attribute& Channel_Attribute::operator =(std::string const& str) {
    int err;
    if ((err = iio_channel_attr_write(dev->a, key.c_str(), str.c_str())) < 0) {
        throw std::system_error{-err, std::generic_category(), "channel attribute write error"};
    }
    return *this;
}

Channel_Attribute& Channel_Attribute::operator =(const char* str) {
    int err;
    if ((err = iio_channel_attr_write(dev->a, key.c_str(), str)) < 0) {
        throw std::system_error{-err, std::generic_category(), "channel attribute write error"};
    }
    return *this;
}

Channel_Attribute& Channel_Attribute::operator = (long long str){
    int err;
    if ((err = iio_channel_attr_write_longlong(dev->a, key.c_str(), str)) < 0) {
        throw std::system_error{-err, std::generic_category(), "channel attribute write error"};
    }
    return *this;
}

Channel_Attribute& Channel_Attribute::operator = (bool str){
    int err;
    if ((err = iio_channel_attr_write_bool(dev->a, key.c_str(), str)) < 0) {
        throw std::system_error{-err, std::generic_category(), "channel attribute write error"};
    }
    return *this;
}

Channel_Attribute& Channel_Attribute::operator = (double str){
    int err;
    if((err = iio_channel_attr_write_double(dev->a, key.c_str(), str)) < 0) {
        throw std::system_error{-err, std::generic_category(), "channel attribute write error"};
    }
    return *this;
}

std::string Channel_Attribute::value() {
    char tmp[MAXATRLENGTH];
    iio_channel_attr_read(dev->a, key.c_str(), tmp, MAXATRLENGTH);
    return std::string(tmp);
}

Channel Device_Channels::operator[] (std::string s) {
    auto ret{iio_device_find_channel(a->dev, s.c_str(), out)};
    if (ret == nullptr) {
        int err = errno;
        throw std::system_error{err, std::generic_category(), "channel not found"};
    }
    return Channel{ret};
}
