#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import math
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import auto_stub
from utils import net


class RetryLoopMockedTest(auto_stub.TestCase):
  """Base class for test cases that mock retry loop."""

  def setUp(self):
    super(RetryLoopMockedTest, self).setUp()
    self._retry_attemps_cls = net.RetryAttempt
    self.mock(net, 'sleep_before_retry', self.mocked_sleep_before_retry)
    self.mock(net, 'current_time', self.mocked_current_time)
    self.mock(net, 'RetryAttempt', self.mocked_retry_attempt)
    self.sleeps = []
    self.attempts = []

  def mocked_sleep_before_retry(self, attempt, max_wait):
    self.sleeps.append((attempt, max_wait))

  def mocked_current_time(self):
    # One attempt is one virtual second.
    return float(len(self.attempts))

  def mocked_retry_attempt(self, *args, **kwargs):
    attempt = self._retry_attemps_cls(*args, **kwargs)
    self.attempts.append(attempt)
    return attempt

  def assertAttempts(self, attempts, max_timeout):
    """Asserts that retry loop executed given number of |attempts|."""
    expected = [(i, max_timeout - i) for i in xrange(attempts)]
    actual = [(x.attempt, x.remaining) for x in self.attempts]
    self.assertEqual(expected, actual)

  def assertSleeps(self, sleeps):
    """Asserts that retry loop slept given number of times."""
    self.assertEqual(sleeps, len(self.sleeps))


class RetryLoopTest(RetryLoopMockedTest):
  """Test for retry_loop implementation."""

  def test_sleep_before_retry(self):
    # Verifies bounds. Because it's using a pseudo-random number generator and
    # not a read random source, it's basically guaranteed to never return the
    # same value twice consecutively.
    a = net.calculate_sleep_before_retry(0, 0)
    b = net.calculate_sleep_before_retry(0, 0)
    self.assertTrue(a >= math.pow(1.5, -1), a)
    self.assertTrue(b >= math.pow(1.5, -1), b)
    self.assertTrue(a < 1.5 + math.pow(1.5, -1), a)
    self.assertTrue(b < 1.5 + math.pow(1.5, -1), b)
    self.assertNotEqual(a, b)


class GetHttpServiceTest(unittest.TestCase):
  """Tests get_http_service implementation."""

  def test_get_http_service(self):
    def assert_is_appengine_service(service):
      """Verifies HttpService is configured for App Engine communications."""
      self.assertTrue(service.use_count_key)
      self.assertIsNotNone(service.authenticator)

    def assert_is_googlestorage_service(service):
      """Verifies HttpService is configured for GS communications."""
      self.assertFalse(service.use_count_key)
      self.assertIsNone(service.authenticator)

    # Can recognize app engine URLs.
    assert_is_appengine_service(
        net.get_http_service('https://appengine-app.appspot.com'))
    assert_is_appengine_service(
        net.get_http_service('https://version-dot-appengine-app.appspot.com'))

    # Localhost is also sort of appengine when running on dev server...
    assert_is_appengine_service(
        net.get_http_service('http://localhost:8080'))

    # Check GS urls.
    assert_is_googlestorage_service(
        net.get_http_service('https://bucket-name.storage.googleapis.com'))


class HttpServiceTest(RetryLoopMockedTest):
  """Tests for HttpService class."""

  @staticmethod
  def mocked_http_service(url='http://example.com', perform_request=None,
                          authenticate=None):  # pylint: disable=R0201
    class MockedAuthenticator(net.Authenticator):
      def authenticate(self):
        return authenticate() if authenticate else None

    class MockedRequestEngine(net.RequestEngine):
      def perform_request(self, request):
        return perform_request(request) if perform_request else None

    return net.HttpService(
        url,
        authenticator=MockedAuthenticator(),
        engine=MockedRequestEngine())

  def test_request_GET_success(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    response = 'True'

    def mock_perform_request(request):
      self.assertTrue(
          request.get_full_url().startswith(service_url + request_url))
      return request.make_fake_response(response)

    service = self.mocked_http_service(url=service_url,
        perform_request=mock_perform_request)
    self.assertEqual(service.request(request_url).read(), response)
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_request_POST_success(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    response = 'True'

    def mock_perform_request(request):
      self.assertTrue(
          request.get_full_url().startswith(service_url + request_url))
      self.assertEqual('', request.body)
      return request.make_fake_response(response)

    service = self.mocked_http_service(url=service_url,
        perform_request=mock_perform_request)
    self.assertEqual(service.request(request_url, data={}).read(), response)
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_request_PUT_success(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    request_body = 'data_body'
    response_body = 'True'
    content_type = 'application/octet-stream'

    def mock_perform_request(request):
      self.assertTrue(
          request.get_full_url().startswith(service_url + request_url))
      self.assertEqual(request_body, request.body)
      self.assertEqual(request.method, 'PUT')
      self.assertEqual(request.headers['Content-Type'], content_type)
      return request.make_fake_response(response_body)

    service = self.mocked_http_service(url=service_url,
        perform_request=mock_perform_request)
    response = service.request(request_url,
        data=request_body, content_type=content_type, method='PUT')
    self.assertEqual(response.read(), response_body)
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)


  def test_request_success_after_failure(self):
    response = 'True'

    def mock_perform_request(request):
      params = dict(request.params)
      if params.get(net.COUNT_KEY) != 1:
        raise net.ConnectionError()
      return request.make_fake_response(response)

    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', data={}).read(), response)
    self.assertAttempts(2, net.URL_OPEN_TIMEOUT)

  def test_request_failure_max_attempts_default(self):
    def mock_perform_request(_request):
      raise net.ConnectionError()
    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/'), None)
    self.assertAttempts(net.URL_OPEN_MAX_ATTEMPTS, net.URL_OPEN_TIMEOUT)

  def test_request_failure_max_attempts(self):
    def mock_perform_request(_request):
      raise net.ConnectionError()
    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', max_attempts=23), None)
    self.assertAttempts(23, net.URL_OPEN_TIMEOUT)

  def test_request_failure_timeout(self):
    def mock_perform_request(_request):
      raise net.ConnectionError()
    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', max_attempts=10000), None)
    self.assertAttempts(int(net.URL_OPEN_TIMEOUT) + 1, net.URL_OPEN_TIMEOUT)

  def test_request_failure_timeout_default(self):
    def mock_perform_request(_request):
      raise net.ConnectionError()
    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', timeout=10.), None)
    self.assertAttempts(11, 10.0)

  def test_request_HTTP_error_no_retry(self):
    count = []
    def mock_perform_request(request):
      count.append(request)
      raise net.HttpError(400)

    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', data={}), None)
    self.assertEqual(1, len(count))
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_request_HTTP_error_retry_404(self):
    response = 'data'

    def mock_perform_request(request):
      params = dict(request.params)
      if params.get(net.COUNT_KEY) == 1:
        return request.make_fake_response(response)
      raise net.HttpError(404)

    service = self.mocked_http_service(perform_request=mock_perform_request)
    result = service.request('/', data={}, retry_404=True)
    self.assertEqual(result.read(), response)
    self.assertAttempts(2, net.URL_OPEN_TIMEOUT)

  def test_request_HTTP_error_with_retry(self):
    response = 'response'

    def mock_perform_request(request):
      params = dict(request.params)
      if params.get(net.COUNT_KEY) != 1:
        raise net.HttpError(500)
      return request.make_fake_response(response)

    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertTrue(service.request('/', data={}).read(), response)
    self.assertAttempts(2, net.URL_OPEN_TIMEOUT)

  def test_auth_success(self):
    count = []
    response = 'response'

    def mock_perform_request(request):
      if not count:
        raise net.HttpError(403)
      return request.make_fake_response(response)

    def mock_authenticate():
      self.assertEqual(len(count), 0)
      count.append(1)
      return True

    service = self.mocked_http_service(perform_request=mock_perform_request,
        authenticate=mock_authenticate)
    self.assertEqual(service.request('/').read(), response)
    self.assertEqual(len(count), 1)
    self.assertAttempts(2, net.URL_OPEN_TIMEOUT)
    self.assertSleeps(0)

  def test_auth_failure(self):
    count = []

    def mock_perform_request(_request):
      raise net.HttpError(403)

    def mock_authenticate():
      count.append(1)
      return False

    service = self.mocked_http_service(perform_request=mock_perform_request,
        authenticate=mock_authenticate)
    self.assertEqual(service.request('/'), None)
    self.assertEqual(len(count), 1)
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_request_attempted_before_auth(self):
    calls = []

    def mock_perform_request(_request):
      calls.append('perform_request')
      raise net.HttpError(403)

    def mock_authenticate():
      calls.append('authenticate')
      return False

    service = self.mocked_http_service(perform_request=mock_perform_request,
        authenticate=mock_authenticate)
    self.assertEqual(service.request('/'), None)
    self.assertEqual(calls, ['perform_request', 'authenticate'])
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_url_read(self):
    # Successfully reads the data.
    self.mock(net, 'url_open',
        lambda url, **_kwargs: net.HttpResponse.get_fake_response('111', url))
    self.assertEqual(net.url_read('https://fake_url.com/test'), '111')

    # Respects url_open connection errors.
    self.mock(net, 'url_open', lambda _url, **_kwargs: None)
    self.assertIsNone(net.url_read('https://fake_url.com/test'))

    # Respects read timeout errors.
    def timeouting_http_response(url):
      def read_mock(_size=None):
        raise net.TimeoutError()
      response = net.HttpResponse.get_fake_response('', url)
      self.mock(response, 'read', read_mock)
      return response

    self.mock(net, 'url_open',
        lambda url, **_kwargs: timeouting_http_response(url))
    self.assertIsNone(net.url_read('https://fake_url.com/test'))


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.FATAL))
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
