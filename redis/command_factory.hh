#pragma once
#include "bytes.hh"
#include "seastar/core/shared_ptr.hh"

namespace service {
class storage_proxy;
}
namespace redis {
using namespace seastar;
class abstract_command;
class request;
class command_factory {
public:
    command_factory() {}
    ~command_factory() {}
    static shared_ptr<abstract_command> create(service::storage_proxy&, request&&);
};
}
