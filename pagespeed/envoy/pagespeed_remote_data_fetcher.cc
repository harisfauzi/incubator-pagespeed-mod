/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "pagespeed_remote_data_fetcher.h"

namespace net_instaweb {

PagespeedRemoteDataFetcher::PagespeedRemoteDataFetcher(Envoy::Upstream::ClusterManager& cm,
                                     const ::envoy::api::v2::core::HttpUri& uri,
                                     PagespeedRemoteDataFetcherCallback& callback)
    : cm_(cm), uri_(uri), callback_(callback) {}

PagespeedRemoteDataFetcher::~PagespeedRemoteDataFetcher() { cancel(); }

void PagespeedRemoteDataFetcher::cancel() {
  if (request_) {
    request_->cancel();
    ENVOY_LOG(debug, "fetch remote data [uri = {}]: canceled", uri_.uri());
  }

  request_ = nullptr;
}

void PagespeedRemoteDataFetcher::fetch() {
  Envoy::Http::MessagePtr message = Envoy::Http::Utility::prepareHeaders(uri_);
  message->headers().insertMethod().value().setReference(Envoy::Http::Headers::get().MethodValues.Get);
  ENVOY_LOG(debug, "fetch remote data from [uri = {}]: start", uri_.uri());
  request_ = cm_.httpAsyncClientForCluster(uri_.cluster())
                 .send(std::move(message), *this,
                       Envoy::Http::AsyncClient::RequestOptions().setTimeout(std::chrono::milliseconds(
                           Envoy::DurationUtil::durationToMilliseconds(uri_.timeout()))));
}

void PagespeedRemoteDataFetcher::onSuccess(Envoy::Http::MessagePtr&& response) {

  // TODO : check for response status
  callback_.onSuccess(response);
  request_ = nullptr;
}

void PagespeedRemoteDataFetcher::onFailure(Envoy::Http::AsyncClient::FailureReason reason) {
  // ENVOY_LOG(debug, "fetch remote data [uri = {}]: network error {}", uri_.uri(), enumToInt(reason));
  std::cout << "PagespeedRemoteDataFetcher::onFailure \n";
  std::cout.flush();
  request_ = nullptr;
  // callback_.onFailure(FailureReason::Network);
}

} // namespace net_instaweb