// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "rpc/rpc-mgr-test-base.h"

using kudu::rpc::ServiceIf;
using kudu::rpc::RpcController;
using kudu::rpc::RpcContext;
using kudu::MonoDelta;

DECLARE_int32(num_reactor_threads);
DECLARE_int32(num_acceptor_threads);
DECLARE_string(hostname);

namespace impala {

// For tests that do not require kerberized testing, we use RpcTest.
class RpcMgrTest : public RpcMgrTestBase<testing::Test> {
  virtual void SetUp() {
    RpcMgrTestBase::SetUp();
  }

  virtual void TearDown() {
    RpcMgrTestBase::TearDown();
  }
};

TEST_F(RpcMgrTest, MultipleServicesTls) {
  // TODO: We're starting a seperate RpcMgr here instead of configuring
  // RpcTestBase::rpc_mgr_ to use TLS. To use RpcTestBase::rpc_mgr_, we need to introduce
  // new gtest params to turn on TLS which needs to be a coordinated change across
  // rpc-mgr-test and thrift-server-test.
  RpcMgr tls_rpc_mgr(IsInternalTlsConfigured());
  TNetworkAddress tls_krpc_address;
  IpAddr ip;
  ASSERT_OK(HostnameToIpAddr(FLAGS_hostname, &ip));

  int32_t tls_service_port = FindUnusedEphemeralPort(nullptr);
  tls_krpc_address = MakeNetworkAddress(ip, tls_service_port);

  ScopedSetTlsFlags s(SERVER_CERT, PRIVATE_KEY, SERVER_CERT);
  ASSERT_OK(tls_rpc_mgr.Init());

  ASSERT_OK(RunMultipleServicesTestTemplate(this, &tls_rpc_mgr, tls_krpc_address));
  tls_rpc_mgr.Shutdown();
}

TEST_F(RpcMgrTest, MultipleServices) {
  ASSERT_OK(RunMultipleServicesTestTemplate(this, &rpc_mgr_, krpc_address_));
}

// Test with a misconfigured TLS certificate and verify that an error is thrown.
TEST_F(RpcMgrTest, BadCertificateTls) {
  ScopedSetTlsFlags s(SERVER_CERT, PRIVATE_KEY, "unknown");

  RpcMgr tls_rpc_mgr(IsInternalTlsConfigured());
  TNetworkAddress tls_krpc_address;
  IpAddr ip;
  ASSERT_OK(HostnameToIpAddr(FLAGS_hostname, &ip));

  int32_t tls_service_port = FindUnusedEphemeralPort(nullptr);
  tls_krpc_address = MakeNetworkAddress(ip, tls_service_port);

  ASSERT_FALSE(tls_rpc_mgr.Init().ok());
  tls_rpc_mgr.Shutdown();
}

// Test with a bad password command for the password protected private key.
TEST_F(RpcMgrTest, BadPasswordTls) {
  ScopedSetTlsFlags s(SERVER_CERT, PASSWORD_PROTECTED_PRIVATE_KEY, SERVER_CERT,
      "echo badpassword");

  RpcMgr tls_rpc_mgr(IsInternalTlsConfigured());
  TNetworkAddress tls_krpc_address;
  IpAddr ip;
  ASSERT_OK(HostnameToIpAddr(FLAGS_hostname, &ip));

  int32_t tls_service_port = FindUnusedEphemeralPort(nullptr);
  tls_krpc_address = MakeNetworkAddress(ip, tls_service_port);

  ASSERT_FALSE(tls_rpc_mgr.Init().ok());
  tls_rpc_mgr.Shutdown();
}

// Test with a correct password command for the password protected private key.
TEST_F(RpcMgrTest, CorrectPasswordTls) {
  ScopedSetTlsFlags s(SERVER_CERT, PASSWORD_PROTECTED_PRIVATE_KEY, SERVER_CERT,
      "echo password");

  RpcMgr tls_rpc_mgr(IsInternalTlsConfigured());
  TNetworkAddress tls_krpc_address;
  IpAddr ip;
  ASSERT_OK(HostnameToIpAddr(FLAGS_hostname, &ip));

  int32_t tls_service_port = FindUnusedEphemeralPort(nullptr);
  tls_krpc_address = MakeNetworkAddress(ip, tls_service_port);

  ASSERT_OK(tls_rpc_mgr.Init());
  ASSERT_OK(RunMultipleServicesTestTemplate(this, &tls_rpc_mgr, tls_krpc_address));
  tls_rpc_mgr.Shutdown();
}

// Test with a bad TLS cipher and verify that an error is thrown.
TEST_F(RpcMgrTest, BadCiphersTls) {
  ScopedSetTlsFlags s(SERVER_CERT, PRIVATE_KEY, SERVER_CERT, "", "not_a_cipher");

  RpcMgr tls_rpc_mgr(IsInternalTlsConfigured());
  TNetworkAddress tls_krpc_address;
  IpAddr ip;
  ASSERT_OK(HostnameToIpAddr(FLAGS_hostname, &ip));

  int32_t tls_service_port = FindUnusedEphemeralPort(nullptr);
  tls_krpc_address = MakeNetworkAddress(ip, tls_service_port);

  ASSERT_FALSE(tls_rpc_mgr.Init().ok());
  tls_rpc_mgr.Shutdown();
}

// Test with a valid TLS cipher.
TEST_F(RpcMgrTest, ValidCiphersTls) {
  ScopedSetTlsFlags s(SERVER_CERT, PRIVATE_KEY, SERVER_CERT, "",
      TLS1_0_COMPATIBLE_CIPHER);

  RpcMgr tls_rpc_mgr(IsInternalTlsConfigured());
  TNetworkAddress tls_krpc_address;
  IpAddr ip;
  ASSERT_OK(HostnameToIpAddr(FLAGS_hostname, &ip));

  int32_t tls_service_port = FindUnusedEphemeralPort(nullptr);
  tls_krpc_address = MakeNetworkAddress(ip, tls_service_port);

  ASSERT_OK(tls_rpc_mgr.Init());
  ASSERT_OK(RunMultipleServicesTestTemplate(this, &tls_rpc_mgr, tls_krpc_address));
  tls_rpc_mgr.Shutdown();
}

// Test with multiple valid TLS ciphers.
TEST_F(RpcMgrTest, ValidMultiCiphersTls) {
  const string cipher_list = Substitute("$0,$1", TLS1_0_COMPATIBLE_CIPHER,
      TLS1_0_COMPATIBLE_CIPHER_2);
  ScopedSetTlsFlags s(SERVER_CERT, PRIVATE_KEY, SERVER_CERT, "", cipher_list);

  RpcMgr tls_rpc_mgr(IsInternalTlsConfigured());
  TNetworkAddress tls_krpc_address;
  IpAddr ip;
  ASSERT_OK(HostnameToIpAddr(FLAGS_hostname, &ip));

  int32_t tls_service_port = FindUnusedEphemeralPort(nullptr);
  tls_krpc_address = MakeNetworkAddress(ip, tls_service_port);

  ASSERT_OK(tls_rpc_mgr.Init());
  ASSERT_OK(RunMultipleServicesTestTemplate(this, &tls_rpc_mgr, tls_krpc_address));
  tls_rpc_mgr.Shutdown();
}

TEST_F(RpcMgrTest, SlowCallback) {
  // Use a callback which is slow to respond.
  auto slow_cb = [](RpcContext* ctx) {
    SleepForMs(300);
    ctx->RespondSuccess();
  };

  // Test a service which is slow to respond and has a short queue.
  // Set a timeout on the client side. Expect either a client timeout
  // or the service queue filling up.
  unique_ptr<ServiceIf> impl(new PingServiceImpl(rpc_mgr_.metric_entity(),
      rpc_mgr_.result_tracker(), service_tracker(), slow_cb));
  const int num_service_threads = 1;
  const int queue_size = 3;
  ASSERT_OK(rpc_mgr_.RegisterService(num_service_threads, queue_size, move(impl),
      service_tracker()));

  FLAGS_num_acceptor_threads = 2;
  FLAGS_num_reactor_threads = 10;
  ASSERT_OK(rpc_mgr_.StartServices(krpc_address_));

  unique_ptr<PingServiceProxy> proxy;
  ASSERT_OK(rpc_mgr_.GetProxy<PingServiceProxy>(krpc_address_, &proxy));

  PingRequestPB request;
  PingResponsePB response;
  RpcController controller;
  for (int i = 0; i < 100; ++i) {
    controller.Reset();
    controller.set_timeout(MonoDelta::FromMilliseconds(50));
    kudu::Status status = proxy->Ping(request, &response, &controller);
    ASSERT_TRUE(status.IsTimedOut() || RpcMgr::IsServerTooBusy(controller));
  }
}

TEST_F(RpcMgrTest, AsyncCall) {
  unique_ptr<ServiceIf> scan_mem_impl(new ScanMemServiceImpl(rpc_mgr_.metric_entity(),
      rpc_mgr_.result_tracker(), service_tracker()));
  ASSERT_OK(rpc_mgr_.RegisterService(10, 10, move(scan_mem_impl), service_tracker()));

  unique_ptr<ScanMemServiceProxy> scan_mem_proxy;
  ASSERT_OK(rpc_mgr_.GetProxy<ScanMemServiceProxy>(krpc_address_, &scan_mem_proxy));

  FLAGS_num_acceptor_threads = 2;
  FLAGS_num_reactor_threads = 10;
  ASSERT_OK(rpc_mgr_.StartServices(krpc_address_));

  RpcController controller;
  srand(0);
  for (int i = 0; i < 10; ++i) {
    controller.Reset();
    ScanMemRequestPB request;
    ScanMemResponsePB response;
    SetupScanMemRequest(&request, &controller);
    CountingBarrier barrier(1);
    scan_mem_proxy->ScanMemAsync(request, &response, &controller,
        [barrier_ptr = &barrier]() { barrier_ptr->Notify(); });
    // TODO: Inject random cancellation here.
    barrier.Wait();
    ASSERT_TRUE(controller.status().ok()) << controller.status().ToString();
  }
}

} // namespace impala

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  impala::InitCommonRuntime(argc, argv, false, impala::TestInfo::BE_TEST);

  // Fill in the path of the current binary for use by the tests.
  CURRENT_EXECUTABLE_PATH = argv[0];
  return RUN_ALL_TESTS();
}
