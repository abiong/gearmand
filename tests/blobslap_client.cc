/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Test the blobslap_client program
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *      * The names of its contributors may not be used to endorse or
 *  promote products derived from this software without specific prior
 *  written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


/*
  Test that we are cycling the servers we are creating during testing.
*/

#include <libtest/test.hpp>

using namespace libtest;

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

#include "tests/ports.h"

static std::string executable;

static test_return_t help_test(void *)
{
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "-p %d", int(default_port()));
  const char *args[]= { buffer, "-?", 0 };

  test_success(exec_cmdline(executable, args));
  return TEST_SUCCESS;
}

static test_return_t unknown_test(void *)
{
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "-p %d", int(default_port()));
  const char *args[]= { buffer, "--unknown", 0 };

  // The argument doesn't exist, so we should see an error
  test_failed(exec_cmdline(executable, args));
  return TEST_SUCCESS;
}

static test_return_t basic_benchmark_test(void *)
{
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "-p %d", int(default_port()));
  const char *args[]= { buffer, "-c 100", "-n 10", "-e", 0 };

  // The argument doesn't exist, so we should see an error
  test_success(exec_cmdline(executable, args));
  return TEST_SUCCESS;
}

test_st benchmark_tests[] ={
  {"--help", 0, help_test},
  {"--unknown", 0, unknown_test},
  {"-c 100 -n 10", 0, basic_benchmark_test},
  {0, 0, 0}
};

collection_st collection[] ={
  {"blobslap_client", 0, 0, benchmark_tests},
  {0, 0, 0, 0}
};

static void *world_create(server_startup_st& servers, test_return_t& error)
{
  const char *argv[1]= { "blobslap_client" };
  if (not server_startup(servers, "gearmand", BLOBSLAP_CLIENT_TEST_PORT, 1, argv))
  {
    error= TEST_FAILURE;
  }

  if (not server_startup(servers, "blobslap_worker", BLOBSLAP_CLIENT_TEST_PORT, 1, argv))
  {
    error= TEST_FAILURE;
  }

  return &servers;
}


void get_world(Framework *world)
{
  executable= "./benchmark/blobslap_client";
  world->collections= collection;
  world->_create= world_create;
}

