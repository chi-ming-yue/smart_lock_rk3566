#include "rknn_common.h"

#include <fstream>
#include <sstream>

bool load_model_file(const std::string &path, std::vector<unsigned char> *bytes, std::string *error_message) {
    if (bytes == NULL || error_message == NULL) {
        return false;
    }

    error_message->clear();
    bytes->clear();

    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        *error_message = "failed to open model file: " + path;
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0) {
        *error_message = "empty model file: " + path;
        return false;
    }
    file.seekg(0, std::ios::beg);

    bytes->resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(&(*bytes)[0]), size);
    if (!file.good() && !file.eof()) {
        *error_message = "failed to read model file: " + path;
        bytes->clear();
        return false;
    }

    return true;
}

std::string tensor_attr_to_string(const rknn_tensor_attr &attr) {
    std::ostringstream stream;
    stream << "index=" << attr.index
           << " n_dims=" << attr.n_dims
           << " dims=[";
    for (unsigned int i = 0; i < attr.n_dims; ++i) {
        if (i != 0) {
            stream << ',';
        }
        stream << attr.dims[i];
    }
    stream << "] n_elems=" << attr.n_elems
           << " size=" << attr.size
           << " fmt=" << attr.fmt
           << " type=" << attr.type
           << " qnt_type=" << attr.qnt_type
           << " zp=" << attr.zp
           << " scale=" << attr.scale;
    return stream.str();
}
