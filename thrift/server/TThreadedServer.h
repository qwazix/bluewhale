/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _THRIFT_SERVER_TTHREADEDSERVER_H_
#define _THRIFT_SERVER_TTHREADEDSERVER_H_ 1

#include "thrift/server/TServer.h"
#include "thrift/transport/TServerTransport.h"
#include "thrift/concurrency/Monitor.h"
#include "thrift/concurrency/Thread.h"

#include <boost/shared_ptr.hpp>

namespace apache { namespace thrift { namespace server {

using apache::thrift::TProcessor;
using apache::thrift::transport::TServerTransport;
using apache::thrift::transport::TTransportFactory;
using apache::thrift::concurrency::Monitor;
using apache::thrift::concurrency::ThreadFactory;

class TThreadedServer : public TServer {

 public:
  class Task;

  TThreadedServer(boost::shared_ptr<TProcessor> processor,
                  boost::shared_ptr<TServerTransport> serverTransport,
                  boost::shared_ptr<TTransportFactory> transportFactory,
                  boost::shared_ptr<TProtocolFactory> protocolFactory);

  TThreadedServer(boost::shared_ptr<TProcessor> processor,
                  boost::shared_ptr<TServerTransport> serverTransport,
                  boost::shared_ptr<TTransportFactory> transportFactory,
                  boost::shared_ptr<TProtocolFactory> protocolFactory,
                  boost::shared_ptr<ThreadFactory> threadFactory);

  virtual ~TThreadedServer();

  virtual void serve();

  void stop() {
    stop_ = true;
    serverTransport_->interrupt();
  }

 protected:
  boost::shared_ptr<ThreadFactory> threadFactory_;
  volatile bool stop_;

  Monitor tasksMonitor_;
  std::set<Task*> tasks_;

};

}}} // apache::thrift::server

#endif // #ifndef _THRIFT_SERVER_TTHREADEDSERVER_H_
