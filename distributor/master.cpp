
#include "master.h"
#include <boost/format.hpp>
#include <glog/logging.h>

namespace ddc {
namespace distributor {

Master::Master(base::ProducerConsumerQueue<base::Block<FullRequest> >* requestQueue,
          base::ProducerConsumerQueue<base::Block<FullRequest> >* responseQueue,
          const uint64_t numRequests) :
    requestQueue_(requestQueue),
    responseQueue_(responseQueue),
    requestsLeft_(numRequests),
    responsesLeft_(numRequests),
    numSplitRequests_(0),
    numSplitResponses_(0),
    numHeartBeatRequests_(0),
    numHeartBeatResponses_(0),
    numRegistrations_(0),
    index_(0),
    heartBeatIndex_(0)
{
}

Master::~Master() {
    LOG(INFO) << " numSplitRequests_: " << numSplitRequests_ <<
                 " numSplitResponses_: " << numSplitResponses_ <<
                 " numHeartBeatRequests_: " << numHeartBeatRequests_ <<
                 " numHeartBeatResponses_: " << numHeartBeatResponses_ <<
                 " numRegistrations_: " << numRegistrations_;
}

void Master::onResponse(const FullRequest& response) {
    AnyRequest_Type type = response.protoMessage.type();


    if(type == AnyRequest_Type_HEARTBEAT_RESPONSE) {
        // this worker is alive, save time
        heartBeatTimeoutTick_[response.worker] = boost::posix_time::microsec_clock::local_time();
        LOG(INFO) << "received heartbeat response from " << response.worker;
        ++numHeartBeatResponses_;
    }
    else if(type == AnyRequest_Type_REGISTRATION) {
        std::string id = response.protoMessage.registration().id();
        LOG(INFO) << "received registration from worker " << response.worker << " identified as " << id;
        ++numRegistrations_;
        // add to registered workers list
        workers_.push_back(response.worker);
        //initialize timers
        heartBeatTimeoutTick_[response.worker] = boost::posix_time::microsec_clock::local_time();
    }
    else if(type == AnyRequest_Type_FETCH_SPLIT_RESPONSE) {
        // update timer
        lackOfProgressTick_ = boost::posix_time::microsec_clock::local_time();
        LOG(INFO) << "received splitresponse for " << response.protoMessage.fetchsplitresponse().filename() << " from worker " << response.worker;
        std::string splitId = response.protoMessage.fetchsplitresponse().filename();
        SplitMapIt it = requestsInFlight_.find(splitId);
        if(it != requestsInFlight_.end()) {
            SplitTrackingInfo &s = it->second;
            if(s.status == Status::PENDING) {
                --responsesLeft_;
                ++numSplitResponses_;
                //TODO handle if the status is ERROR e.g. rescheduling to another worker
                if (response.protoMessage.fetchsplitresponse().status() == 0) s.status = Status::OK;
                else s.status = Status::ERROR;
            }
            else {
                LOG(ERROR) << "received response for split " << s.id << " that wasn't pending. its status was " << s.status;
            }
         }
         else {
             LOG(ERROR) << "received unknown response for split" << splitId << " from worker " << response.worker;
         }
    }
}


void Master::onWorkerDead(const std::string& worker) {
    LOG(INFO) << "worker " << worker << " is dead";

    //remove inflight reqs from dead worker
    for(SplitMapIt it = requestsInFlight_.begin(); it != requestsInFlight_.end(); it++) {
        //std::string& splitId = it->first;
        SplitTrackingInfo &s = it->second;
        if((s.worker == worker) && (s.status == Status::PENDING)) {
            //reschedule split
            LOG(INFO) << "rescheduling split " << s.id;
            requests_.push_back(SplitTrackingInfo(s.id));
        }
        else if((s.worker == worker) && (s.status != Status::PENDING)) {
            //in this case maybe the worker send the response before dying
            LOG(INFO) << "trying to mark split " << s.id << " but it's not pending it's " << s.status;
        }
    }

    // remove worker from list
    // important to do after the checks above
    for(int i = 0; i < workers_.size(); i++) {
        if(workers_[i] == worker) {
            workers_.erase(workers_.begin() + i);
            break;
        }
    }
}

std::string Master::getRoundRobinWorker() {

    std::string chosen =  workers_[index_ % workers_.size()];
    ++index_;
    return chosen;
}

void Master::run()
{
    LOG(INFO) << "starting master";

    //initialize timers

    //used to send heartbeats periodically
    boost::posix_time::ptime heartBeatSendTick = boost::posix_time::microsec_clock::local_time();
    for(int i = 0; i < workers_.size(); i++) {
        //used to check for dead workers
        heartBeatTimeoutTick_[workers_[i]] = boost::posix_time::microsec_clock::local_time();
    }

    //handle lack of progress
    lackOfProgressTick_ = boost::posix_time::microsec_clock::local_time();

    //create the request list
    for(int i = 0; i < requestsLeft_; i++) {
        std::string filename = (boost::format("split%d") %(i)).str();
        requests_.push_back(SplitTrackingInfo(filename));
    }


    // TODO if we insert to many splitFetchRequests in between heartbeats
    // we cannot guarantee hearbeats are sent exactly every kHeartBeatSendFrequency_

    while((requests_.size() > 0) ||
          (responsesLeft_ > 0)) {

        // tick
        boost::posix_time::ptime now  = boost::posix_time::microsec_clock::local_time();

        // check for lack of progress
        boost::posix_time::time_duration lackOfProgressDiff = now - lackOfProgressTick_;
        if(lackOfProgressDiff.total_milliseconds() > kLackOfProgressTimeout_) {
            LOG(ERROR) << "lack of progress, exiting ...";
            goto end;
        }

        /**
         * Send heartbeat requests
         */
        boost::posix_time::time_duration heartBeatSendDiff = now - heartBeatSendTick;
        uint64_t tmp = heartBeatSendDiff.total_milliseconds();

        if(tmp > kHeartBeatSendFrequency_) {
            base::Block<FullRequest> *heartBeatRequest;
            for(int i = 0; i < workers_.size(); i++) {
                while(1) {
                    //try until we send heartbeats for all the workers
                    if(requestQueue_->tryGetWriteSlot(&heartBeatRequest)) {
                        //send hearbeat and reset
                        AnyRequest req;
                        req.set_type(AnyRequest_Type_HEARTBEAT_REQUEST);
                        HeartBeatRequest *r = new HeartBeatRequest;
                        req.set_allocated_heartbeatrequest(r);
                        heartBeatRequest->data.protoMessage = req;

                        heartBeatRequest->data.worker = workers_[heartBeatIndex_ % workers_.size()];
                        LOG(INFO) << "sending heartbeat to worker " << heartBeatRequest->data.worker;
                        ++heartBeatIndex_;
                        requestQueue_->slotWritten(heartBeatRequest);
                        heartBeatSendTick = boost::posix_time::microsec_clock::local_time();
                        ++numHeartBeatRequests_;
                        break;
                    } // if
                }  // while(1)
            }  // for every worker
        } // if

        /**
         * Check Heartbeats
         */
        for(int i = 0; i < workers_.size(); i++) {
            boost::posix_time::time_duration heartBeatTimeoutDiff = now - heartBeatTimeoutTick_[workers_[i]];
            if(heartBeatTimeoutDiff.total_milliseconds() > kHeartBeatTimeout_) {
                onWorkerDead(workers_[i]);
            }
        }

        /**
         * send next split requests
         */
        // while there are remaining splits and available workers send requests
        if((requests_.size() > 0) && (workers_.size() > 0)) {
            base::Block<FullRequest> *splitRequest;
            if(requestQueue_->tryGetWriteSlot(&splitRequest)) {
                SplitTrackingInfo& s = requests_.front();
                AnyRequest req;
                req.set_type(AnyRequest_Type_FETCH_SPLIT_REQUEST);
                FetchSplitRequest *r = new FetchSplitRequest;
                r->set_filename(s.id);
                req.set_allocated_fetchsplitrequest(r);
                splitRequest->data.protoMessage = req;
                //TODO choose worker with the least splits outstanding instead of roundrobin
                splitRequest->data.worker = getRoundRobinWorker();
                requestsInFlight_.insert(
                            std::pair<std::string,SplitTrackingInfo>(s.id, SplitTrackingInfo(s.id, splitRequest->data.worker)));
                requestQueue_->slotWritten(splitRequest);
                LOG(INFO) << "sending split " << s.id << " to worker " << splitRequest->data.worker;
                ++numSplitRequests_;
                requests_.pop_front();
            }
        }

        /**
         * receive next response
         */
        base::Block<FullRequest> *response;
        if(responseQueue_->tryGetReadSlot(&response)) {
            onResponse(response->data);
            responseQueue_->slotRead(response);
        }
    }  // while((requests_.size() > 0) || (responsesLeft_ > 0))

    /**
     * shutdown workers
     */
    for(int i = 0; i < workers_.size(); i++) {
        base::Block<FullRequest> *shutdown;
        while(1) {
            if(requestQueue_->tryGetWriteSlot(&shutdown)) {
                AnyRequest req;
                req.set_type(AnyRequest_Type_SHUTDOWN_REQUEST);
                ShutdownRequest *r = new ShutdownRequest;
                req.set_allocated_shutdownrequest(r);
                shutdown->data.protoMessage = req;
                shutdown->data.worker = workers_[i];
                requestQueue_->slotWritten(shutdown);
                LOG(INFO) << "sent shutdown";
                break;
            }
        }
    }

    //give some time so the shutdown messages get flushed from the queues
    //TODO maybe implement shutdown acks or a way to know when the queues are empty
    boost::this_thread::sleep(boost::posix_time::seconds(5));

end:
    uint64_t okSplits = 0;

    /**
     * check for errors
     */

    for(SplitMapIt it = requestsInFlight_.begin(); it != requestsInFlight_.end(); it++) {
        SplitTrackingInfo& s = it->second;
        if(s.status == Status::ERROR) {
            LOG(ERROR) << "error on split " << s.id;
        }
        else if(s.status == Status::PENDING) {
            LOG(ERROR) << "pending split " << s.id;
        }
        else if(s.status == Status::OK) {
            ++okSplits;
        }
    }

    LOG(INFO) << "processed " << requestsInFlight_.size() << " splits";
    LOG(INFO) << "ok: " << okSplits << " out of " << numSplitRequests_;
}
} // namespace distributor
} // namespace ddc
