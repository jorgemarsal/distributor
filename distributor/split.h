
#ifndef DDC_DISTRIBUTOR_SPLIT_H
#define DDC_DISTRIBUTOR_SPLIT_H

#include <map>
#include <string>

#include "distributor.pb.h"

namespace ddc {
namespace distributor {

class Status{
public:
    enum value {
        PENDING,
        OK,
        ERROR
    };
};



struct SplitTrackingInfo {
    SplitTrackingInfo(const std::string& _id) :
        status(Status::PENDING),
        worker(""),
        id(_id)
    {

    }

    SplitTrackingInfo(const std::string& _id,
                      const std::string& _worker) :
        status(Status::PENDING),
        worker(_worker),
        id(_id)
    {

    }

    Status::value status;
    std::string worker;
    std::string id;
};

struct FullRequest {
    AnyRequest protoMessage;
    std::string worker; // worker this req is addressed to

};

typedef std::map<std::string, SplitTrackingInfo> SplitMap;
typedef std::map<std::string, SplitTrackingInfo>::const_iterator SplitMapConstIt;
typedef std::map<std::string, SplitTrackingInfo>::iterator SplitMapIt;



} // namespace distributor
} // namespace ddc

#endif // DDC_DISTRIBUTOR_SPLIT_H
