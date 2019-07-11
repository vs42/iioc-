#include "iio.h"
#include <complex>
#include <vector>
#include <string>
#include <assert.h>

class Buffer;
class Device;
class Context;
class Channel;
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
    Device_Attribute& operator =(std::string str);
    Device_Attribute& operator =(long long str);
    Device_Attribute& operator =(double str);
    Device_Attribute& operator =(bool str);
    Device_Attribute& operator =(Device_Attribute d);
    bool operator ==(Device_Attribute d);
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
    Device_Attribute operator[] (std::string& s);
};

class Device {
    iio_device *dev;
public:
    friend Buffer;
    friend Device_Attributes;
    friend Device_Attribute;
    Device_Attributes attributes;
    Device(iio_device* device) : attributes(this) {
        dev = device;
    }
    size_t sample_size() {
        return iio_device_get_sample_size(dev);
    }
    std::string id();
    std::string name();
    Channel* find_channel(std::string s, bool output);
};

class Buffer {
    iio_buffer* a;
    std::vector<std::complex<int16_t>> v;
public:
    ptrdiff_t step() const{
        return iio_buffer_step(a);
    }

    Buffer(Device *dev, size_t samples_count = 1024*1024, bool cyclic = false) {
        a = iio_device_create_buffer(dev->dev, samples_count, cyclic);
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

class Context_Devices {
    Context* a;
public:
    Context_Devices(Context* b) {
        a = b;
    }
    int size();
    Device* operator[] (unsigned int i);
    Device* operator[] (std::string s);
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
    Channel_Attribute& operator =(std::string str);
    Channel_Attribute& operator =(long long str);
    Channel_Attribute& operator =(double str);
    Channel_Attribute& operator =(bool str);
    Channel_Attribute& operator =(Channel_Attribute d);
    bool operator ==(Channel_Attribute d);
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

    Channel (iio_channel *b) : attributes(this) {
        a = b;
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
    ~Channel() {
        this->disable();
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

Device* Context_Devices::operator[] (unsigned int i) {
    auto d = new Device(iio_context_get_device(a->a, i));
    return d;
}

Device* Context_Devices::operator[] (std::string s) {
    auto d = new Device(iio_context_find_device(a->a, s.c_str()));
    return d;
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

Device_Attribute Device_Attributes::operator[] (std::string& s) {
    return Device_Attribute(s, a);
}

Channel* Device::find_channel(std::string s, bool output) {
    auto c = new Channel(iio_device_find_channel(dev, s.c_str(), output));
    return c;
}

Device_Attribute& Device_Attribute::operator =(std::string str) {
    iio_device_attr_write(dev->dev, key.c_str(), str.c_str());
    return *this;
}

Device_Attribute& Device_Attribute::operator = (long long str){
    iio_device_attr_write_longlong(dev->dev, key.c_str(), str);
    return *this;
}

Device_Attribute& Device_Attribute::operator = (bool str){
    iio_device_attr_write_bool(dev->dev, key.c_str(), str);
    return *this;
}

Device_Attribute& Device_Attribute::operator = (double str){
    iio_device_attr_write_double(dev->dev, key.c_str(), str);
    return *this;
}

Device_Attribute& Device_Attribute::operator =(Device_Attribute d) {
    iio_device_attr_write(dev->dev, key.c_str(), iio_device_find_attr(d.dev->dev, d.key.c_str()));
    return *this;
}
bool Device_Attribute::operator ==(Device_Attribute d) {
    return iio_device_find_attr(dev->dev, key.c_str()) == iio_device_find_attr(d.dev->dev, d.key.c_str());
}

std::string Device_Attribute::value() {
    return std::string(iio_device_find_attr(dev->dev, key.c_str()));
}

Channel_Attribute& Channel_Attribute::operator =(std::string str) {
    iio_channel_attr_write(dev->a, key.c_str(), str.c_str());
    return *this;
}

Channel_Attribute& Channel_Attribute::operator = (long long str){
    iio_channel_attr_write_longlong(dev->a, key.c_str(), str);
    return *this;
}

Channel_Attribute& Channel_Attribute::operator = (bool str){
    iio_channel_attr_write_bool(dev->a, key.c_str(), str);
    return *this;
}

Channel_Attribute& Channel_Attribute::operator = (double str){
    iio_channel_attr_write_double(dev->a, key.c_str(), str);
    return *this;
}

Channel_Attribute& Channel_Attribute::operator =(Channel_Attribute d) {
    iio_channel_attr_write(dev->a, key.c_str(), iio_channel_find_attr(d.dev->a, d.key.c_str()));
    return *this;
}
bool Channel_Attribute::operator ==(Channel_Attribute d) {
    return iio_channel_find_attr(dev->a, key.c_str()) == iio_channel_find_attr(d.dev->a, d.key.c_str());
}

std::string Channel_Attribute::value() {
    return std::string(iio_channel_find_attr(dev->a, key.c_str()));
}
