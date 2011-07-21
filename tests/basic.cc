/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
 *  All rights reserved.
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

#include <config.h>
#include <libtest/test.hpp>

using namespace libtest;

#include <cstring>
#include <cassert>

#include <libgearman/gearman.h>

#include <tests/basic.h>
#include <tests/context.h>
#include <tests/start_worker.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
pthread_mutex_t counter_lock= PTHREAD_MUTEX_INITIALIZER;

struct counter_st
{
  int32_t _count;
  pthread_mutex_t _lock;

  counter_st() :
    _count(0)
  {
    pthread_mutex_init(&_lock, NULL);
  }

  void increment()
  {
    pthread_mutex_lock(&_lock);
    _count++;
    pthread_mutex_unlock(&_lock);
  }

  int32_t count()
  {
    int32_t tmp;
    pthread_mutex_lock(&_lock);
    tmp= _count;
    pthread_mutex_unlock(&_lock);

    return tmp;
  }
};

static gearman_return_t counter_function2(gearman_job_st *, void *object)
{
  counter_st *count= (counter_st*)object;
  assert(count);
  count->increment();

  return GEARMAN_SUCCESS;
}

/* Counter test for worker */
static void *counter_function(gearman_job_st *,
                              void *context, size_t *result_size,
                              gearman_return_t *)
{
  uint32_t *counter= (uint32_t *)context;

  *result_size= 0;

  pthread_mutex_lock(&counter_lock);
  *counter= *counter + 1;
  pthread_mutex_unlock(&counter_lock);

  return NULL;
}

test_return_t client_echo_fail_test(void *object)
{
  Context *test= (Context *)object;
  test_truth(test);

  gearman_client_st client, *client_ptr;

  client_ptr= gearman_client_create(&client);
  test_truth(client_ptr);

  test_true_got(gearman_success(gearman_client_add_server(&client, NULL, 20)), gearman_client_error(client_ptr));

  gearman_return_t rc= gearman_client_echo(&client, test_literal_param("This should never work"));
  test_true_got(gearman_failed(rc), gearman_strerror(rc));

  gearman_client_free(&client);

  return TEST_SUCCESS;
}

test_return_t client_echo_test(void *object)
{
  Context *test= (Context *)object;
  test_truth(test);

  gearman_client_st client, *client_ptr;

  client_ptr= gearman_client_create(&client);
  test_truth(client_ptr);

  test_true_got(gearman_success(gearman_client_add_server(&client, NULL, test->port())), gearman_client_error(client_ptr));

  gearman_return_t rc= gearman_client_echo(&client, test_literal_param("This is my echo test"));
  test_true_got(rc == GEARMAN_SUCCESS, gearman_strerror(rc));

  gearman_client_free(&client);

  return TEST_SUCCESS;
}

test_return_t worker_echo_test(void *object)
{
  Context *test= (Context *)object;
  test_truth(test);

  gearman_worker_st *worker= test->worker;
  test_truth(worker);

  gearman_return_t rc= gearman_worker_echo(worker, test_literal_param("This is my echo test"));
  test_true_got(rc == GEARMAN_SUCCESS, gearman_strerror(rc));

  return TEST_SUCCESS;
}

test_return_t queue_clean(void *object)
{
  Context *test= (Context *)object;
  test_truth(test);
  gearman_worker_st *worker= test->worker;
  test_truth(worker);

  gearman_worker_set_timeout(worker, 1000);

  uint32_t counter= 0;
  test_true_got(gearman_success(gearman_worker_add_function(worker, test->worker_function_name(), 5, counter_function, &counter)), gearman_worker_error(worker));

  // Clean out any jobs that might still be in the queue from failed tests.
  while (1)
  {
    gearman_return_t rc= gearman_worker_work(worker);
    if (rc != GEARMAN_SUCCESS)
      break;
  }

  return TEST_SUCCESS;
}

test_return_t queue_add(void *object)
{
  Context *test= (Context *)object;
  gearman_client_st client, *client_ptr;
  gearman_job_handle_t job_handle= {};
  test_truth(test);

  test->run_worker= false;

  client_ptr= gearman_client_create(&client);
  test_truth(client_ptr);

  test_true_got(gearman_success(gearman_client_add_server(&client, NULL, test->port())), gearman_client_error(client_ptr));

  test_compare(GEARMAN_SUCCESS,
               gearman_client_echo(&client, test_literal_param("background_payload")));

  test_compare(GEARMAN_SUCCESS,
               gearman_client_do_background(&client, test->worker_function_name(), NULL, 
                                            test_literal_param("background_payload"),
                                            job_handle));
  test_truth(job_handle[0]);

  gearman_return_t rc;
  do {
    rc= gearman_client_job_status(client_ptr, job_handle, NULL, NULL, NULL, NULL);
    test_true(rc != GEARMAN_IN_PROGRESS);
  } while (gearman_continue(rc) and rc != GEARMAN_JOB_EXISTS); // We need to exit on these values since the job will never run
  test_true(rc == GEARMAN_JOB_EXISTS or rc == GEARMAN_SUCCESS);

  gearman_client_free(&client);

  test->run_worker= true;

  return TEST_SUCCESS;
}

test_return_t queue_worker(void *object)
{
  Context *test= (Context *)object;
  test_truth(test);

  // Setup job
  test_compare(TEST_SUCCESS, queue_add(object));

  gearman_worker_st *worker= test->worker;
  test_truth(worker);

  test_true_got(test->run_worker, "run_worker was not set");

  uint32_t counter= 0;
  test_compare_got(GEARMAN_SUCCESS, 
                   gearman_worker_add_function(worker, test->worker_function_name(), 5, counter_function, &counter),
                   gearman_worker_error(worker));

  gearman_worker_set_timeout(worker, 1000);

  gearman_return_t rc= gearman_worker_work(worker);
  test_compare_got(GEARMAN_SUCCESS, rc, (rc == GEARMAN_TIMEOUT) ? "Worker was not able to connection to the server, is it running?": gearman_strerror(rc));

  test_truth(counter);

  return TEST_SUCCESS;
}

#define NUMBER_OF_WORKERS 10
#define NUMBER_OF_JOBS 40
#define JOB_SIZE 100
test_return_t lp_734663(void *object)
{
  Context *test= (Context *)object;
  test_truth(test);

  const char *worker_function_name= "drizzle_queue_test";

  uint8_t value[JOB_SIZE]= { 'x' };
  memset(&value, 'x', sizeof(value));

  gearman_client_st client, *client_ptr;
  client_ptr= gearman_client_create(&client);
  test_truth(client_ptr);

  test_true_got(gearman_success(gearman_client_add_server(&client, NULL, test->port())), gearman_client_error(client_ptr));

  test_compare(GEARMAN_SUCCESS, gearman_client_echo(&client, value, sizeof(JOB_SIZE)));

  for (uint32_t x= 0; x < NUMBER_OF_JOBS; x++)
  {
    gearman_job_handle_t job_handle= {};
    test_compare(GEARMAN_SUCCESS, gearman_client_do_background(&client, worker_function_name, NULL, value, sizeof(value), job_handle));
    test_truth(job_handle[0]);
  }

  gearman_client_free(&client);

  struct worker_handle_st *worker_handle[NUMBER_OF_WORKERS];

  counter_st counter;
  gearman_function_t counter_function_fn= gearman_function_create(counter_function2);
  for (uint32_t x= 0; x < NUMBER_OF_WORKERS; x++)
  {
    worker_handle[x]= test_worker_start(test->port(), NULL, worker_function_name, counter_function_fn, &counter, gearman_worker_options_t());
  }

  while (counter.count() < NUMBER_OF_JOBS)
  {
#ifdef WIN32
    sleep(gearman_timeout(worker)/100000);
#else
    struct timespec global_sleep_value= { 0, static_cast<long>(gearman_timeout(client_ptr) *1000) };
    nanosleep(&global_sleep_value, NULL);
#endif
  }

  for (uint32_t x= 0; x < NUMBER_OF_WORKERS; x++)
  {
    worker_handle[x]->shutdown();
  }

  for (uint32_t x= 0; x < NUMBER_OF_WORKERS; x++)
  {
    delete worker_handle[x];
  }

  return TEST_SUCCESS;
}
