//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "agent/confront.hh"
#include "test/asserts.hh"
#include "test/unittest.hh"

using namespace conprx;

TEST(protocol, ntstatus) {
  // STATUS_SUCCESS
  ASSERT_EQ(0x00000000, NtStatus::success().to_nt());
  ASSERT_EQ(0x00000000, NtStatus::from(NtStatus::nsSuccess, NtStatus::npMs, 0).to_nt());
  // STATUS_WAIT_63
  ASSERT_EQ(0x0000003F, NtStatus::from(NtStatus::nsSuccess, NtStatus::npMs, 63).to_nt());
  // STATUS_FILE_NOT_AVAILABLE
  ASSERT_EQ(0xC0000467, NtStatus::from(NtStatus::nsError, NtStatus::npMs, 0x467).to_nt());
  // STATUS_THREAD_WAS_SUSPENDED
  ASSERT_EQ(0x40000001, NtStatus::from(NtStatus::nsInfo, NtStatus::npMs, 1).to_nt());
}
