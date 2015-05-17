
#ifndef DDC_DISTRIBUTOR_WORKER_H
#define DDC_DISTRIBUTOR_WORKER_H

#include "base/runnable.h"

namespace ddc {
namespace distributor {

class Worker: public base::Runnable  {
public:
    void run();
};

} // namespace distributor
} // namespace ddc

#endif // DDC_DISTRIBUTOR_WORKER_H
