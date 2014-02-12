// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "content/public/test/unittest_test_suite.h"

int main(int argc, char **argv) {
  content::UnitTestTestSuite test_suite(new ChromeTestSuite(argc, argv));

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&content::UnitTestTestSuite::Run,
                             base::Unretained(&test_suite)));
}
