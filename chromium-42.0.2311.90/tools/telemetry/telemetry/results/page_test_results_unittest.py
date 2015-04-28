# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import page as page_module
from telemetry.page import page_set
from telemetry.results import base_test_results_unittest
from telemetry.results import page_test_results
from telemetry.timeline import trace_data
from telemetry.value import failure
from telemetry.value import histogram
from telemetry.value import scalar
from telemetry.value import skip
from telemetry.value import trace

class PageTestResultsTest(base_test_results_unittest.BaseTestResultsUnittest):
  def setUp(self):
    ps = page_set.PageSet(file_path=os.path.dirname(__file__))
    ps.AddUserStory(page_module.Page("http://www.bar.com/", ps, ps.base_dir))
    ps.AddUserStory(page_module.Page("http://www.baz.com/", ps, ps.base_dir))
    ps.AddUserStory(page_module.Page("http://www.foo.com/", ps, ps.base_dir))
    self.page_set = ps

  @property
  def pages(self):
    return self.page_set.pages

  def testFailures(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    results.AddValue(
        failure.FailureValue(self.pages[0], self.CreateException()))
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    results.DidRunPage(self.pages[1])

    self.assertEqual(set([self.pages[0]]), results.pages_that_failed)
    self.assertEqual(set([self.pages[1]]), results.pages_that_succeeded)

    self.assertEqual(2, len(results.all_page_runs))
    self.assertTrue(results.all_page_runs[0].failed)
    self.assertTrue(results.all_page_runs[1].ok)


  def testSkips(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    results.AddValue(skip.SkipValue(self.pages[0], 'testing reason'))
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    results.DidRunPage(self.pages[1])

    self.assertTrue(results.all_page_runs[0].skipped)
    self.assertEqual(self.pages[0], results.all_page_runs[0].page)
    self.assertEqual(set([self.pages[0], self.pages[1]]),
                     results.pages_that_succeeded)

    self.assertEqual(2, len(results.all_page_runs))
    self.assertTrue(results.all_page_runs[0].skipped)
    self.assertTrue(results.all_page_runs[1].ok)

  def testBasic(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    results.AddValue(scalar.ScalarValue(self.pages[1], 'a', 'seconds', 3))
    results.DidRunPage(self.pages[1])

    results.PrintSummary()

    values = results.FindPageSpecificValuesForPage(self.pages[0], 'a')
    self.assertEquals(1, len(values))
    v = values[0]
    self.assertEquals(v.name, 'a')
    self.assertEquals(v.page, self.pages[0])

    values = results.FindAllPageSpecificValuesNamed('a')
    assert len(values) == 2

  def testResultsFiltering(self):
    def AcceptValueNamed_a(value):
      return value.name == 'a'
    results = page_test_results.PageTestResults(
        value_can_be_added_predicate=AcceptValueNamed_a)
    results.WillRunPage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.AddValue(scalar.ScalarValue(self.pages[0], 'b', 'seconds', 3))
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    results.AddValue(scalar.ScalarValue(self.pages[1], 'a', 'seconds', 3))
    results.AddValue(scalar.ScalarValue(self.pages[1], 'd', 'seconds', 3))
    results.DidRunPage(self.pages[1])

    results.PrintSummary()

    values = results.FindPageSpecificValuesForPage(self.pages[0], 'a')
    self.assertEquals(1, len(values))
    v = values[0]
    self.assertEquals(v.name, 'a')
    self.assertEquals(v.page, self.pages[0])

    values = results.FindPageSpecificValuesForPage(self.pages[0], 'b')
    self.assertEquals(0, len(values))

    values = results.FindAllPageSpecificValuesNamed('a')
    self.assertEquals(len(values), 2)

    values = results.all_page_specific_values
    self.assertEquals(len(values), 2)

  def testUrlIsInvalidValue(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    self.assertRaises(
      AssertionError,
      lambda: results.AddValue(scalar.ScalarValue(
          self.pages[0], 'url', 'string', 'foo')))

  def testAddSummaryValueWithPageSpecified(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    self.assertRaises(
      AssertionError,
      lambda: results.AddSummaryValue(scalar.ScalarValue(self.pages[0],
                                                         'a', 'units', 3)))

  def testUnitChange(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    self.assertRaises(
      AssertionError,
      lambda: results.AddValue(scalar.ScalarValue(
          self.pages[1], 'a', 'foobgrobbers', 3)))

  def testTypeChange(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    self.assertRaises(
      AssertionError,
      lambda: results.AddValue(histogram.HistogramValue(
          self.pages[1], 'a', 'seconds',
          raw_value_json='{"buckets": [{"low": 1, "high": 2, "count": 1}]}')))

  def testGetPagesThatSucceededAllPagesFail(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    results.AddValue(failure.FailureValue.FromMessage(self.pages[0], 'message'))
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    results.AddValue(scalar.ScalarValue(self.pages[1], 'a', 'seconds', 7))
    results.AddValue(failure.FailureValue.FromMessage(self.pages[1], 'message'))
    results.DidRunPage(self.pages[1])

    results.PrintSummary()
    self.assertEquals(0, len(results.pages_that_succeeded))

  def testGetSuccessfulPageValuesMergedNoFailures(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    results.AddValue(scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3))
    self.assertEquals(1, len(results.all_page_specific_values))
    results.DidRunPage(self.pages[0])

  def testGetAllValuesForSuccessfulPages(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    value1 = scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3)
    results.AddValue(value1)
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    value2 = scalar.ScalarValue(self.pages[1], 'a', 'seconds', 3)
    results.AddValue(value2)
    results.DidRunPage(self.pages[1])

    results.WillRunPage(self.pages[2])
    value3 = scalar.ScalarValue(self.pages[2], 'a', 'seconds', 3)
    results.AddValue(value3)
    results.DidRunPage(self.pages[2])

    self.assertEquals(
        [value1, value2, value3], results.all_page_specific_values)

  def testGetAllValuesForSuccessfulPagesOnePageFails(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    value1 = scalar.ScalarValue(self.pages[0], 'a', 'seconds', 3)
    results.AddValue(value1)
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    value2 = failure.FailureValue.FromMessage(self.pages[1], 'Failure')
    results.AddValue(value2)
    results.DidRunPage(self.pages[1])

    results.WillRunPage(self.pages[2])
    value3 = scalar.ScalarValue(self.pages[2], 'a', 'seconds', 3)
    results.AddValue(value3)
    results.DidRunPage(self.pages[2])

    self.assertEquals(
        [value1, value2, value3], results.all_page_specific_values)

  def testTraceValue(self):
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.pages[0])
    results.AddValue(trace.TraceValue(None, trace_data.TraceData({'test' : 1})))
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    results.AddValue(trace.TraceValue(None, trace_data.TraceData({'test' : 2})))
    results.DidRunPage(self.pages[1])

    results.PrintSummary()

    values = results.FindAllTraceValues()
    self.assertEquals(2, len(values))

  def testCleanUpCleansUpTraceValues(self):
    results = page_test_results.PageTestResults()
    v0 = trace.TraceValue(None, trace_data.TraceData({'test': 1}))
    v1 = trace.TraceValue(None, trace_data.TraceData({'test': 2}))

    results.WillRunPage(self.pages[0])
    results.AddValue(v0)
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    results.AddValue(v1)
    results.DidRunPage(self.pages[1])

    results.CleanUp()
    self.assertTrue(v0.cleaned_up)
    self.assertTrue(v1.cleaned_up)

  def testNoTracesLeftAfterCleanUp(self):
    results = page_test_results.PageTestResults()
    v0 = trace.TraceValue(None, trace_data.TraceData({'test': 1}))
    v1 = trace.TraceValue(None, trace_data.TraceData({'test': 2}))

    results.WillRunPage(self.pages[0])
    results.AddValue(v0)
    results.DidRunPage(self.pages[0])

    results.WillRunPage(self.pages[1])
    results.AddValue(v1)
    results.DidRunPage(self.pages[1])

    results.CleanUp()
    self.assertFalse(results.FindAllTraceValues())
