// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mock_proxy_resolver.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"

namespace net {

MockAsyncProxyResolverBase::Request::Request(
    MockAsyncProxyResolverBase* resolver,
    const GURL& url,
    ProxyInfo* results,
    const CompletionCallback& callback)
    : resolver_(resolver),
      url_(url),
      results_(results),
      callback_(callback),
      origin_loop_(base::MessageLoop::current()) {}

    void MockAsyncProxyResolverBase::Request::CompleteNow(int rv) {
      CompletionCallback callback = callback_;

      // May delete |this|.
      resolver_->RemovePendingRequest(this);

      callback.Run(rv);
    }

MockAsyncProxyResolverBase::Request::~Request() {}

MockAsyncProxyResolverBase::SetPacScriptRequest::SetPacScriptRequest(
    MockAsyncProxyResolverBase* resolver,
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    const CompletionCallback& callback)
    : resolver_(resolver),
      script_data_(script_data),
      callback_(callback),
      origin_loop_(base::MessageLoop::current()) {}

MockAsyncProxyResolverBase::SetPacScriptRequest::~SetPacScriptRequest() {}

    void MockAsyncProxyResolverBase::SetPacScriptRequest::CompleteNow(int rv) {
       CompletionCallback callback = callback_;

      // Will delete |this|.
      resolver_->RemovePendingSetPacScriptRequest(this);

      callback.Run(rv);
    }

MockAsyncProxyResolverBase::~MockAsyncProxyResolverBase() {}

int MockAsyncProxyResolverBase::GetProxyForURL(
    const GURL& url, ProxyInfo* results, const CompletionCallback& callback,
    RequestHandle* request_handle, const BoundNetLog& /*net_log*/) {
  scoped_refptr<Request> request = new Request(this, url, results, callback);
  pending_requests_.push_back(request);

  if (request_handle)
    *request_handle = reinterpret_cast<RequestHandle>(request.get());

  // Test code completes the request by calling request->CompleteNow().
  return ERR_IO_PENDING;
}

void MockAsyncProxyResolverBase::CancelRequest(RequestHandle request_handle) {
  scoped_refptr<Request> request = reinterpret_cast<Request*>(request_handle);
  cancelled_requests_.push_back(request);
  RemovePendingRequest(request.get());
}

LoadState MockAsyncProxyResolverBase::GetLoadState(
    RequestHandle request_handle) const {
  return LOAD_STATE_RESOLVING_PROXY_FOR_URL;
}

int MockAsyncProxyResolverBase::SetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    const CompletionCallback& callback) {
  DCHECK(!pending_set_pac_script_request_.get());
  pending_set_pac_script_request_.reset(
      new SetPacScriptRequest(this, script_data, callback));
  // Finished when user calls SetPacScriptRequest::CompleteNow().
  return ERR_IO_PENDING;
}

void MockAsyncProxyResolverBase::CancelSetPacScript() {
  // Do nothing (caller was responsible for completing the request).
}

MockAsyncProxyResolverBase::SetPacScriptRequest*
MockAsyncProxyResolverBase::pending_set_pac_script_request() const {
  return pending_set_pac_script_request_.get();
}

void MockAsyncProxyResolverBase::RemovePendingRequest(Request* request) {
  RequestsList::iterator it = std::find(
      pending_requests_.begin(), pending_requests_.end(), request);
  DCHECK(it != pending_requests_.end());
  pending_requests_.erase(it);
}

void MockAsyncProxyResolverBase::RemovePendingSetPacScriptRequest(
    SetPacScriptRequest* request) {
  DCHECK_EQ(request, pending_set_pac_script_request());
  pending_set_pac_script_request_.reset();
}

MockAsyncProxyResolverBase::MockAsyncProxyResolverBase(bool expects_pac_bytes)
    : ProxyResolver(expects_pac_bytes) {}

MockAsyncProxyResolverFactory::Request::Request(
    MockAsyncProxyResolverFactory* factory,
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    scoped_ptr<ProxyResolver>* resolver,
    const CompletionCallback& callback)
    : factory_(factory),
      script_data_(script_data),
      resolver_(resolver),
      callback_(callback) {
}

MockAsyncProxyResolverFactory::Request::~Request() {
}

void MockAsyncProxyResolverFactory::Request::CompleteNow(
    int rv,
    scoped_ptr<ProxyResolver> resolver) {
  *resolver_ = resolver.Pass();

  // RemovePendingRequest may remove the last external reference to |this|.
  scoped_refptr<MockAsyncProxyResolverFactory::Request> keep_alive(this);
  factory_->RemovePendingRequest(this);
  factory_ = nullptr;
  callback_.Run(rv);
}

void MockAsyncProxyResolverFactory::Request::CompleteNowWithForwarder(
    int rv,
    ProxyResolver* resolver) {
  DCHECK(resolver);
  CompleteNow(rv, make_scoped_ptr(new ForwardingProxyResolver(resolver)));
}

void MockAsyncProxyResolverFactory::Request::FactoryDestroyed() {
  factory_ = nullptr;
}

class MockAsyncProxyResolverFactory::Job
    : public ProxyResolverFactory::Request {
 public:
  explicit Job(
      const scoped_refptr<MockAsyncProxyResolverFactory::Request>& request)
      : request_(request) {}
  ~Job() override {
    if (request_->factory_) {
      request_->factory_->cancelled_requests_.push_back(request_);
      request_->factory_->RemovePendingRequest(request_.get());
    }
  }

 private:
  scoped_refptr<MockAsyncProxyResolverFactory::Request> request_;
};

MockAsyncProxyResolverFactory::MockAsyncProxyResolverFactory(
    bool resolvers_expect_pac_bytes)
    : ProxyResolverFactory(resolvers_expect_pac_bytes) {
}

int MockAsyncProxyResolverFactory::CreateProxyResolver(
    const scoped_refptr<ProxyResolverScriptData>& pac_script,
    scoped_ptr<ProxyResolver>* resolver,
    const net::CompletionCallback& callback,
    scoped_ptr<ProxyResolverFactory::Request>* request_handle) {
  scoped_refptr<Request> request =
      new Request(this, pac_script, resolver, callback);
  pending_requests_.push_back(request);

  request_handle->reset(new Job(request));

  // Test code completes the request by calling request->CompleteNow().
  return ERR_IO_PENDING;
}

void MockAsyncProxyResolverFactory::RemovePendingRequest(Request* request) {
  RequestsList::iterator it =
      std::find(pending_requests_.begin(), pending_requests_.end(), request);
  DCHECK(it != pending_requests_.end());
  pending_requests_.erase(it);
}

MockAsyncProxyResolverFactory::~MockAsyncProxyResolverFactory() {
  for (auto& request : pending_requests_) {
    request->FactoryDestroyed();
  }
}

ForwardingProxyResolver::ForwardingProxyResolver(ProxyResolver* impl)
    : ProxyResolver(impl->expects_pac_bytes()), impl_(impl) {
}

int ForwardingProxyResolver::GetProxyForURL(const GURL& query_url,
                                            ProxyInfo* results,
                                            const CompletionCallback& callback,
                                            RequestHandle* request,
                                            const BoundNetLog& net_log) {
  return impl_->GetProxyForURL(query_url, results, callback, request, net_log);
}

void ForwardingProxyResolver::CancelRequest(RequestHandle request) {
  impl_->CancelRequest(request);
}

LoadState ForwardingProxyResolver::GetLoadState(RequestHandle request) const {
  return impl_->GetLoadState(request);
}

void ForwardingProxyResolver::CancelSetPacScript() {
  impl_->CancelSetPacScript();
}

int ForwardingProxyResolver::SetPacScript(
    const scoped_refptr<ProxyResolverScriptData>& script_data,
    const CompletionCallback& callback) {
  return impl_->SetPacScript(script_data, callback);
}

}  // namespace net
