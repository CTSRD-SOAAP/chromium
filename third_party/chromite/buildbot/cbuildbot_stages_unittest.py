#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

import contextlib
import copy
import cPickle
import json
import mox
import os
import signal
import StringIO
import sys
import tempfile
import time

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_config as config
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import cbuildbot_stages as stages
from chromite.buildbot import lab_status
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import manifest_version_unittest
from chromite.buildbot import portage_utilities
from chromite.buildbot import repository
from chromite.buildbot import validation_pool
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import git_unittest
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.scripts import cbuildbot

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock

# pylint: disable=E1111,E1120,W0212,R0901,R0904
class StageTest(cros_test_lib.MoxTempDirTestCase,
                cros_test_lib.MockTestCase):
  TARGET_MANIFEST_BRANCH = 'ooga_booga'
  BUILDROOT = 'buildroot'

  def setUp(self):
    self.bot_id = 'x86-generic-paladin'
    self.build_config = copy.deepcopy(config.config[self.bot_id])
    self.build_root = os.path.join(self.tempdir, self.BUILDROOT)
    self._boards = self.build_config['boards']
    self._current_board = self._boards[0]

    self.url = 'fake_url'
    self.build_config['manifest_repo_url'] = self.url

    # Use the cbuildbot parser to create properties and populate default values.
    parser = cbuildbot._CreateParser()
    (self.options, _) = parser.parse_args(
        ['-r', self.build_root, '--buildbot', '--noprebuilts',
          '--buildnumber', '1234'])

    self.assertEquals(self.options.buildroot, self.build_root)
    self.assertTrue(self.options.buildbot)
    self.assertFalse(self.options.debug)
    self.assertFalse(self.options.prebuilts)
    self.assertFalse(self.options.clobber)
    self.assertEquals(self.options.buildnumber, 1234)
    self.options.debug_forced = self.options.debug

    bs.BuilderStage.SetManifestBranch(self.TARGET_MANIFEST_BRANCH)
    portage_utilities._OVERLAY_LIST_CMD = '/bin/true'

  def AutoPatch(self, to_patch):
    """Patch a list of objects with autospec=True.

    Arguments:
      to_patch: A list of tuples in the form (target, attr) to patch.  Will be
      directly passed to mock.patch.object.
    """
    for item in to_patch:
      self.PatchObject(*item, autospec=True)


class AbstractStageTest(StageTest):
  """Base class for tests that test a particular build stage.

  Abstract base class that sets up the build config and options with some
  default values for testing BuilderStage and its derivatives.
  """

  def ConstructStage(self):
    """Returns an instance of the stage to be tested.
    Implement in subclasses.
    """
    raise NotImplementedError(self, "ConstructStage: Implement in your test")

  def RunStage(self):
    """Creates and runs an instance of the stage to be tested.
    Requires ConstructStage() to be implemented.

    Raises:
      NotImplementedError: ConstructStage() was not implemented.
    """

    # Stage construction is usually done as late as possible because the tests
    # set up the build configuration and options used in constructing the stage.
    results_lib.Results.Clear()
    stage = self.ConstructStage()
    stage.Run()
    self.assertTrue(results_lib.Results.BuildSucceededSoFar())


def patch(*args, **kwargs):
  """Convenience wrapper for mock.patch.object.

  Sets autospec=True by default.
  """
  kwargs.setdefault('autospec', True)
  return mock.patch.object(*args, **kwargs)


@contextlib.contextmanager
def patches(*args):
  """Context manager for a list of patch objects."""
  with cros_build_lib.ContextManagerStack() as stack:
    for arg in args:
      stack.Add(lambda: arg)
    yield


class BuilderStageTest(AbstractStageTest):

  def ConstructStage(self):
    return bs.BuilderStage(self.options, self.build_config)

  def testGetPortageEnvVar(self):
    """Basic test case for _GetPortageEnvVar function."""
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    envvar = 'EXAMPLE'
    obj = cros_test_lib.EasyAttr(output='RESULT\n')
    cros_build_lib.RunCommand(mox.And(mox.IsA(list), mox.In(envvar)),
                              cwd='%s/src/scripts' % self.build_root,
                              redirect_stdout=True, enter_chroot=True,
                              error_code_ok=True).AndReturn(obj)
    self.mox.ReplayAll()
    stage = self.ConstructStage()
    board = self._current_board
    result = stage._GetPortageEnvVar(envvar, board)
    self.mox.VerifyAll()
    self.assertEqual(result, 'RESULT')


class ManifestVersionedSyncStageTest(AbstractStageTest):
  """Tests the two (heavily related) stages ManifestVersionedSync, and
     ManifestVersionedSyncCompleted.
  """
  # pylint: disable=W0223

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'branch'

    self.build_config['manifest_version'] = self.manifest_version_url
    self.next_version = 'next_version'

    repo = repository.RepoRepository(
      self.source_repo, self.tempdir, self.branch)
    self.manager = manifest_version.BuildSpecsManager(
      repo, self.manifest_version_url, self.build_name, self.incr_type,
      force=False, branch=self.branch, dry_run=True)

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager
    self.sync_stage = stages.ManifestVersionedSyncStage(
        self.options, self.build_config)

  def testManifestVersionedSyncOnePartBranch(self):
    """Tests basic ManifestVersionedSyncStage with branch ooga_booga"""
    self.mox.StubOutWithMock(stages.ManifestVersionedSyncStage,
                             'Initialize')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetNextBuildSpec')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetLatestPassingSpec')
    self.mox.StubOutWithMock(stages.SyncStage, 'ManifestCheckout')

    stages.ManifestVersionedSyncStage.Initialize()
    self.manager.GetNextBuildSpec().AndReturn(self.next_version)
    self.manager.GetLatestPassingSpec().AndReturn(None)

    stages.SyncStage.ManifestCheckout(self.next_version)

    self.mox.ReplayAll()
    self.sync_stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedSuccess(self):
    """Tests basic ManifestVersionedSyncStageCompleted on success"""

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager

    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    self.manager.UpdateStatus(message=None, success=True)

    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.options,
                                                        self.build_config,
                                                        self.sync_stage,
                                                        success=True)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedFailure(self):
    """Tests basic ManifestVersionedSyncStageCompleted on failure"""

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager

    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    self.manager.UpdateStatus(message=None, success=False)


    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.options,
                                                        self.build_config,
                                                        self.sync_stage,
                                                        success=False)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedIncomplete(self):
    """Tests basic ManifestVersionedSyncStageCompleted on incomplete build."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.options,
                                                        self.build_config,
                                                        self.sync_stage,
                                                        success=False)
    stage.Run()
    self.mox.VerifyAll()


class LKGMCandidateSyncCompletionStage(AbstractStageTest):
  """Tests the two (heavily related) stages ManifestVersionedSync, and
     ManifestVersionedSyncCompleted.
  """

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'x86-generic-paladin'
    self.build_type = constants.PFQ_TYPE

    self.build_config['manifest_version'] = True
    self.build_config['build_type'] = self.build_type
    self.build_config['master'] = True

    repo = repository.RepoRepository(
      self.source_repo, self.tempdir, self.branch)
    self.manager = lkgm_manager.LKGMManager(
      repo, self.manifest_version_url, self.build_name, self.build_type,
      incr_type='branch', force=False, branch=self.branch, dry_run=True)
    self.sub_manager = lkgm_manager.LKGMManager(
      repo, self.manifest_version_url, self.build_name, self.build_type,
      incr_type='branch', force=False, branch=self.branch, dry_run=True)

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager
    stages.LKGMCandidateSyncStage.sub_manager = self.manager

  def ConstructStage(self):
    sync_stage = stages.LKGMCandidateSyncStage(self.options, self.build_config)
    return stages.LKGMCandidateSyncCompletionStage(self.options,
                                                   self.build_config,
                                                   sync_stage,
                                                   success=True)

  def _GetTestConfig(self):
    test_config = {}
    test_config['test1'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': False,
        'chrome_rev': None,
        'branch': False,
        'internal': False,
    }
    test_config['test2'] = {
        'manifest_version': False,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'branch': False,
        'internal': False,
    }
    test_config['test3'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'both',
        'important': True,
        'chrome_rev': None,
        'branch': False,
        'internal': True,
    }
    test_config['test4'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'both',
        'important': True,
        'chrome_rev': None,
        'branch': True,
        'internal': True,
    }
    test_config['test5'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'branch': False,
        'internal': False,
    }
    return test_config

  def testGetSlavesForMaster(self):
    """Tests that we get the slaves for a fake unified master configuration."""
    test_config = self._GetTestConfig()
    self.mox.ReplayAll()
    stage = self.ConstructStage()
    p = stage._GetSlavesForMaster(self.build_config, test_config)
    self.mox.VerifyAll()

    self.assertTrue(test_config['test3'] in p)
    self.assertTrue(test_config['test5'] in p)
    self.assertFalse(test_config['test1'] in p)
    self.assertFalse(test_config['test2'] in p)
    self.assertFalse(test_config['test4'] in p)


class AbstractBuildTest(AbstractStageTest,
                        cros_build_lib_unittest.RunCommandTestCase):
  # pylint: disable=W0223
  def runBuild(self, dir_exists, full, extra_config=None):
    """Helper for running the build."""
    self.bot_id = 'x86-generic-full' if full else 'x86-generic-paladin'
    self.build_config = copy.deepcopy(config.config[self.bot_id])
    if extra_config:
      self.build_config.update(extra_config)
    with mock.patch.object(os.path, 'isdir', return_value=dir_exists):
      self.RunStage()


class InitSDKTest(AbstractBuildTest):
  """Test building the SDK"""

  def ConstructStage(self):
    return stages.InitSDKStage(self.options, self.build_config)

  def testFullBuildWithExistingChroot(self):
    """Tests whether we create chroots for full builds."""
    self.runBuild(dir_exists=True, full=True)
    self.assertCommandContains(['cros_sdk'])

  def testBinBuildWithMissingChroot(self):
    """Tests whether we create chroots when needed."""
    self.runBuild(dir_exists=False, full=False)
    self.assertCommandContains(['cros_sdk'])

  def testFullBuildWithMissingChroot(self):
    """Tests whether we create chroots when needed."""
    self.runBuild(dir_exists=True, full=True)
    self.assertCommandContains(['cros_sdk'])

  def testBinBuildWithNoSDK(self):
    """Tests whether the --nosdk option works."""
    self.options.nosdk = True
    self.runBuild(dir_exists=False, full=False)
    self.assertCommandContains(['cros_sdk', '--bootstrap'])

  def testBinBuildWithExistingChroot(self):
    """Tests whether the --nosdk option works."""
    self.runBuild(dir_exists=True, full=False)
    self.assertCommandContains(['cros_sdk'], expected=False)


class SetupBoardTest(AbstractBuildTest):
  """Test building the board"""

  def ConstructStage(self):
    return stages.SetupBoardStage(self.options, self.build_config)

  def runFullBuild(self, dir_exists=False, extra_config=None):
    """Helper for testing a full builder."""
    self.runBuild(dir_exists, full=True, extra_config=extra_config)
    cmd = ['./setup_board', '--board=%s' % self._current_board,
           '--nousepkg']
    self.assertCommandContains(cmd, expected=not dir_exists)
    cmd = ['./setup_board', '--skip_chroot_upgrade']
    self.assertCommandContains(cmd, expected=False)

  def testFullBuildWithProfile(self):
    """Tests whether full builds add profile flag when requested."""
    self.runFullBuild(dir_exists=False, extra_config={'profile': 'foo'})
    self.assertCommandContains(['./setup_board', '--profile=foo'])

  def testFullBuildWithOverriddenProfile(self):
    """Tests whether full builds add overridden profile flag when requested."""
    self.options.profile = 'smock'
    self.runFullBuild(dir_exists=False)
    self.assertCommandContains(['./setup_board', '--profile=smock'])

  def testFullBuildWithLatestToolchain(self):
    """Tests whether we use --nousepkg for creating the board"""
    self.options.latest_toolchain = True
    self.runFullBuild(dir_exists=False)

  def runBinBuild(self, dir_exists):
    """Helper for testing a binary builder."""
    self.runBuild(dir_exists, full=False)
    self.assertCommandContains(['./setup_board'])
    cmd = ['./setup_board', '--nousepkg']
    self.assertCommandContains(cmd, expected=self.options.latest_toolchain)
    cmd = ['./setup_board', '--skip_chroot_upgrade']
    self.assertCommandContains(cmd, expected=False)

  def testBinBuildWithBoard(self):
    """Tests whether we don't create the board when it's there."""
    self.runBinBuild(dir_exists=True)

  def testBinBuildWithMissingBoard(self):
    """Tests whether we create the board when it's missing."""
    self.runBinBuild(dir_exists=False)

  def testBinBuildWithLatestToolchain(self):
    """Tests whether we use --nousepkg for creating the board."""
    self.options.latest_toolchain = True
    self.runBinBuild(dir_exists=False)

  def testSDKBuild(self):
    """Tests whether we use --skip_chroot_upgrade for SDK builds."""
    extra_config = {'build_type': constants.CHROOT_BUILDER_TYPE}
    self.runBuild(dir_exists=False, full=True, extra_config=extra_config)
    self.assertCommandContains(['./setup_board', '--skip_chroot_upgrade'])


class SDKStageTest(AbstractStageTest):
  """Tests SDK package and Manifest creation."""
  fake_packages = [('cat1/package', '1'), ('cat1/package', '2'),
                   ('cat2/package', '3'), ('cat2/package', '4')]
  fake_json_data = {}
  fake_chroot = None

  def setUp(self):
    self.build_root = self.tempdir
    self.options.buildroot = self.build_root

    # Replace SudoRunCommand, since we don't care about sudo.
    self._OriginalSudoRunCommand = cros_build_lib.SudoRunCommand
    cros_build_lib.SudoRunCommand = cros_build_lib.RunCommand

    # Prepare a fake chroot.
    self.fake_chroot = os.path.join(self.build_root, 'chroot/build/amd64-host')
    os.makedirs(self.fake_chroot)
    osutils.Touch(os.path.join(self.fake_chroot, 'file'))
    for package, v in self.fake_packages:
      cpv = portage_utilities.SplitCPV('%s-%s' % (package, v))
      key = '%s/%s' % (cpv.category, cpv.package)
      self.fake_json_data.setdefault(key, []).append([v, {}])

  def tearDown(self):
    cros_build_lib.SudoRunCommand = self._OriginalSudoRunCommand

  def ConstructStage(self):
    return stages.SDKPackageStage(self.options, self.build_config)

  def testTarballCreation(self):
    """Tests whether we package the tarball and correctly create a Manifest."""
    self.bot_id = 'chromiumos-sdk'
    self.build_config = config.config[self.bot_id]
    fake_tarball = os.path.join(self.build_root, 'built-sdk.tar.xz')
    fake_manifest = os.path.join(self.build_root,
                                 'built-sdk.tar.xz.Manifest')
    self.mox.StubOutWithMock(portage_utilities, 'ListInstalledPackages')
    self.mox.StubOutWithMock(stages.SDKPackageStage,
                             'CreateRedistributableToolchains')

    portage_utilities.ListInstalledPackages(self.fake_chroot).AndReturn(
        self.fake_packages)
    # This code has its own unit tests, so no need to go testing it here.
    stages.SDKPackageStage.CreateRedistributableToolchains(mox.IgnoreArg())

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

    # Check tarball for the correct contents.
    output = cros_build_lib.RunCommandCaptureOutput(
        ['tar', '-I', 'xz', '-tvf', fake_tarball]).output.splitlines()
    # First line is './', use it as an anchor, count the chars, and strip as
    # much from all other lines.
    stripchars = len(output[0]) - 1
    tar_lines = [x[stripchars:] for x in output]
    # TODO(ferringb): replace with assertIn.
    self.assertFalse('/build/amd64-host/' in tar_lines)
    self.assertTrue('/file' in tar_lines)
    # Verify manifest contents.
    real_json_data = json.loads(osutils.ReadFile(fake_manifest))
    self.assertEqual(real_json_data['packages'],
                     self.fake_json_data)


class VMTestStageTest(AbstractStageTest):

  def setUp(self):
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id].copy()
    for cmd in ('RunTestSuite', 'CreateTestRoot', 'GenerateStackTraces',
                'ArchiveFile', 'ArchiveTestResults', 'UploadArchivedFile',
                'RunDevModeTest'):
      self.PatchObject(commands, cmd, autospec=True)
    self.StartPatcher(ArchiveStageMock())

  def ConstructStage(self):
    archive_stage = stages.ArchiveStage(self.options, self.build_config,
                                        self._current_board, '')
    return stages.VMTestStage(self.options, self.build_config,
                              self._current_board, archive_stage)

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self.build_config['vm_tests'] = constants.FULL_AU_TEST_TYPE
    self.RunStage()

  def testQuickTests(self):
    """Tests if quick unit and cros_au_test_harness tests are run correctly."""
    self.build_config['vm_tests'] = constants.SIMPLE_AU_TEST_TYPE
    self.RunStage()


class UnitTestStageTest(AbstractStageTest):

  def setUp(self):
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id].copy()
    self.mox.StubOutWithMock(commands, 'RunUnitTests')

  def ConstructStage(self):
    return stages.UnitTestStage(self.options, self.build_config,
                                self._current_board)

  def testQuickTests(self):
    self.build_config['quick_unit'] = True
    commands.RunUnitTests(self.build_root, self._current_board, full=False,
                          nowithdebug=mox.IgnoreArg(), blacklist=[])
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self.build_config['quick_unit'] = False
    commands.RunUnitTests(self.build_root, self._current_board, full=True,
                          nowithdebug=mox.IgnoreArg(), blacklist=[])
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class HWTestStageTest(AbstractStageTest):

  def setUp(self):
    self.bot_id = 'x86-mario-release'
    self.options.log_dir = '/b/cbuild/mylogdir'
    self.build_config = config.config[self.bot_id].copy()
    self.StartPatcher(ArchiveStageMock())
    self.suite_config = self.build_config['hw_tests'][0]
    self.suite = self.suite_config.suite
    self.archive_stage = stages.ArchiveStage(self.options, self.build_config,
                                             self._current_board, '')
    self.mox.StubOutWithMock(lab_status, 'CheckLabStatus')
    self.mox.StubOutWithMock(commands, 'HaveHWTestsBeenAborted')
    self.mox.StubOutWithMock(commands, 'RunHWTestSuite')
    self.mox.StubOutWithMock(cros_build_lib, 'PrintBuildbotStepWarnings')
    self.mox.StubOutWithMock(cros_build_lib, 'PrintBuildbotStepFailure')
    self.mox.StubOutWithMock(cros_build_lib, 'Warning')
    self.mox.StubOutWithMock(cros_build_lib, 'Error')

  def ConstructStage(self):
    return stages.HWTestStage(self.options, self.build_config,
                              self._current_board, self.archive_stage,
                              self.suite_config)

  def _RunHWTestSuite(self, debug=False, returncode=0, fails=False):
    """Pretend to run the HWTest suite to assist with tests.

    Args:
      debug: Whether the HWTest suite should be run in debug mode.
      returncode: The return value of the HWTest command.
      fails: Whether the command as a whole should fail.
    """
    lab_status.CheckLabStatus(mox.IgnoreArg())
    m = commands.RunHWTestSuite(mox.IgnoreArg(),
                                self.suite,
                                self._current_board, mox.IgnoreArg(),
                                mox.IgnoreArg(), mox.IgnoreArg(), True, debug)

    # Raise an exception if the user wanted the command to fail.
    if returncode != 0:
      result = cros_build_lib.CommandResult(cmd='run_hw_tests',
                                            returncode=returncode)
      m.AndRaise(cros_build_lib.RunCommandError('HWTests failed', result))

      # Make sure failures are logged correctly.
      if fails:
        commands.HaveHWTestsBeenAborted(self.archive_stage.release_tag)
        cros_build_lib.PrintBuildbotStepFailure()
        cros_build_lib.Error(mox.IgnoreArg())
      else:
        cros_build_lib.PrintBuildbotStepWarnings()
        cros_build_lib.Warning(mox.IgnoreArg())

    self.mox.ReplayAll()
    if fails:
      self.assertRaises(results_lib.StepFailure, self.RunStage)
    else:
      self.RunStage()
    self.mox.VerifyAll()

  def _SetupRemoteTrybotConfig(self, args):
    """Setup a remote trybot config with the specified arguments.

    Args:
      args: Command-line arguments to pass in to remote trybot.
    """
    argv = ['--remote-trybot', '-r', self.build_root, self.bot_id] + args
    parser = cbuildbot._CreateParser()
    (self.options, _args) = cbuildbot._ParseCommandLine(parser, argv)
    self.build_config = config.OverrideConfigForTrybot(self.build_config,
                                                       self.options)

  def testRemoteTrybotWithHWTest(self):
    """Test remote trybot with hw test enabled"""
    self._SetupRemoteTrybotConfig(['--hwtest'])
    self._RunHWTestSuite()

  def testRemoteTrybotNoHWTest(self):
    """Test remote trybot with no hw test"""
    self._SetupRemoteTrybotConfig([])
    self._RunHWTestSuite(debug=True)

  def testWithSuite(self):
    """Test if run correctly with a test suite."""
    self._RunHWTestSuite()

  def testWithSuiteWithInfrastructureFailure(self):
    """Tests that we warn correctly if we get a returncode of 2."""
    self._RunHWTestSuite(returncode=2)

  def testWithSuiteWithFatalFailure(self):
    """Tests that we fail if we get a returncode of 1."""
    self._RunHWTestSuite(returncode=1, fails=True)

  def testSendPerfResults(self):
    """Tests that we can send perf results back correctly."""
    self.suite = 'perf_v2'
    self.bot_id = 'lumpy-chrome-perf'
    self.build_config = config.config['lumpy-chrome-perf'].copy()
    self.suite_config = self.build_config['hw_tests'][0]
    self.mox.StubOutWithMock(stages.HWTestStage, '_PrintFile')

    results_file = 'perf_v2.results'
    stages.HWTestStage._PrintFile(os.path.join(self.options.log_dir,
                                               results_file))
    with gs_unittest.GSContextMock() as gs_mock:
      gs_mock.SetDefaultCmdResult()
      self._RunHWTestSuite()

  def testHandleLabDownAsWarning(self):
    """Test that buildbot warn when lab is down."""
    commands.HaveHWTestsBeenAborted(self.archive_stage.release_tag)
    check_lab = lab_status.CheckLabStatus(mox.IgnoreArg())
    check_lab.AndRaise(lab_status.LabIsDownException('Lab is not up.'))
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning(mox.IgnoreArg())
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class AUTestStageTest(AbstractStageTest,
                      cros_build_lib_unittest.RunCommandTestCase):
  """Test only custom methods in AUTestStageTest."""

  def setUp(self):
    self.bot_id = 'x86-mario-release'
    self.build_config = config.config[self.bot_id].copy()
    self.archive_mock = ArchiveStageMock()
    self.StartPatcher(self.archive_mock)
    self.PatchObject(commands, 'ArchiveFile', autospec=True,
                     return_value='foo.txt')
    self.PatchObject(commands, 'HaveHWTestsBeenAborted', autospec=True,
                     return_value=False)
    self.PatchObject(lab_status, 'CheckLabStatus', autospec=True)
    self.archive_stage = stages.ArchiveStage(self.options, self.build_config,
                                             self._current_board, '0.0.1')
    self.suite_config = self.build_config['hw_tests'][0]
    self.suite = self.suite_config.suite

  def ConstructStage(self):
    return stages.AUTestStage(self.options, self.build_config,
                              self._current_board, self.archive_stage,
                              self.suite_config)

  def testPerformStage(self):
    """Tests that we correctly generate a tarball and archive it."""
    stage = self.ConstructStage()
    stage.PerformStage()
    cmd = ['site_utils/autoupdate/full_release_test.py', '--npo', '--dump',
           '--archive_url', self.archive_stage.upload_url,
           self.archive_stage.release_tag, self._current_board]
    self.assertCommandContains(cmd)
    self.assertCommandContains([commands._AUTOTEST_RPC_CLIENT, self.suite])


class UprevStageTest(AbstractStageTest):

  def setUp(self):
    # Disable most paths by default and selectively enable in tests
    self.build_config['uprev'] = False
    self.mox.StubOutWithMock(commands, 'UprevPackages')

  def ConstructStage(self):
    return stages.UprevStage(self.options, self.build_config)

  def testBuildRev(self):
    """Uprevving the build without uprevving chrome."""
    self.build_config['uprev'] = True
    commands.UprevPackages(self.build_root, self._boards, [], enter_chroot=True)
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testNoRev(self):
    """No paths are enabled."""
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class ArchivingMock(partial_mock.PartialMock):
  """Partial mock for ArchivingStage."""

  TARGET = 'chromite.buildbot.cbuildbot_stages.ArchivingStage'
  ATTRS = ('UploadArtifact',)

  def UploadArtifact(self, *args, **kwargs):
    with patch(commands, 'ArchiveFile', return_value='foo.txt'):
      self.backup['UploadArtifact'](*args, **kwargs)


class BuildPackagesStageTest(AbstractStageTest):
  """Tests BuildPackagesStage."""

  def setUp(self):
    self._release_tag = None
    self.StartPatcher(ArchiveStageMock())

  def ConstructStage(self):
    archive_stage = stages.ArchiveStage(self.options, self.build_config,
                                        self._current_board,
                                        self._release_tag)
    return stages.BuildPackagesStage(
        self.options, self.build_config, self._current_board, archive_stage)

  @contextlib.contextmanager
  def RunStageWithConfig(self, bot_id):
    """Run the given config"""
    self.bot_id = bot_id
    self.build_config = copy.deepcopy(config.config[self.bot_id])
    try:
      with cros_build_lib_unittest.RunCommandMock() as rc:
        rc.SetDefaultCmdResult()
        with cros_test_lib.OutputCapturer():
          with cros_test_lib.LoggingCapturer():
            with osutils.TempDir(set_global=True) as tempdir:
              self.build_root = tempdir
              self.options.buildroot = tempdir
              self.RunStage()
        yield self.build_config, rc
    except AssertionError as ex:
      msg = '%s failed the following test:\n%s' % (bot_id, ex)
      raise AssertionError(msg)

  def RunTestsWithConfig(self, bot_id):
    """Test the specified config."""
    with self.RunStageWithConfig(bot_id) as (cfg, rc):
      rc.assertCommandContains(['./build_packages'])
      rc.assertCommandContains(['./build_packages', '--skip_chroot_upgrade'])
      rc.assertCommandContains(['./build_packages', '--nousepkg'],
                               expected=not cfg['usepkg_build_packages'])
      rc.assertCommandContains(['./build_packages', '--nowithdebug'],
                               expected=cfg['nowithdebug'])
      build_tests = cfg['build_tests'] and self.options.tests
      rc.assertCommandContains(['./build_packages', '--nowithautotest'],
                               expected=not build_tests)

  def testAllConfigs(self):
    """Test all major configurations"""
    task = self.RunTestsWithConfig
    with parallel.BackgroundTaskRunner(task) as queue:
      for bot_type in config.CONFIG_TYPE_DUMP_ORDER:
        for bot_id in config.config:
          if bot_id.endswith(bot_type):
            queue.put([bot_id])
            break

  def testNoTests(self):
    """Test that self.options.tests = False works."""
    self.options.tests = False
    self.RunTestsWithConfig('x86-generic-paladin')


class BuildImageStageMock(ArchivingMock):
  """Partial mock for BuildImageStage."""

  TARGET = 'chromite.buildbot.cbuildbot_stages.BuildImageStage'
  ATTRS = ArchivingMock.ATTRS + ('_BuildAutotestTarballs', '_BuildImages',
                                 '_GenerateAuZip')

  def _BuildAutotestTarballs(self, *args, **kwargs):
    with patches(
        patch(commands, 'BuildTarball'),
        patch(commands, 'FindFilesWithPattern', return_value=['foo.txt'])):
      self.backup['_BuildAutotestTarballs'](*args, **kwargs)

  def _BuildImages(self, *args, **kwargs):
    with patches(
        patch(os, 'symlink'),
        patch(os, 'readlink', return_value='foo.txt')):
      self.backup['_BuildImages'](*args, **kwargs)

  def _GenerateAuZip(self, *args, **kwargs):
    with patch(git, 'ReinterpretPathForChroot', return_value='/chroot/path'):
      self.backup['_GenerateAuZip'](*args, **kwargs)


class BuildImageStageTest(BuildPackagesStageTest):
  """Tests BuildImageStage."""

  def setUp(self):
    self.StartPatcher(BuildImageStageMock())

  def ConstructStage(self):
    archive_stage = stages.ArchiveStage(self.options, self.build_config,
                                        self._current_board,
                                        self._release_tag)
    return stages.BuildImageStage(
        self.options, self.build_config, self._current_board,
        archive_stage)

  def RunTestsWithReleaseConfig(self, bot_id, release_tag):
    self._release_tag = release_tag
    with parallel_unittest.ParallelMock():
      with BuildPackagesStageTest.RunStageWithConfig(self, bot_id) as (cfg, rc):
        cmd = ['./build_image', '--version=%s' % (self._release_tag or '')]
        rc.assertCommandContains(cmd, expected=cfg['images'])
        rc.assertCommandContains(['./image_to_vm.sh'],
                                 expected=cfg['vm_tests'])
        hw = cfg['upload_hw_test_artifacts']
        canary = (cfg['build_type'] == constants.CANARY_TYPE)
        rc.assertCommandContains(['--full_payload'], expected=hw and not canary)
        rc.assertCommandContains(['--nplus1'], expected=hw and canary)
        cmd = ['./build_library/generate_au_zip.py', '-o', '/chroot/path']
        rc.assertCommandContains(cmd, expected=cfg['images'])

  def RunTestsWithConfig(self, bot_id):
    """Test the specified config."""
    task = self.RunTestsWithReleaseConfig
    steps = [lambda: task(bot_id, tag) for tag in (None, '0.0.1')]
    parallel.RunParallelSteps(steps)


class ArchiveStageMock(partial_mock.PartialMock):
  """Partial mock for Archive Stage."""

  TARGET = 'chromite.buildbot.cbuildbot_stages.ArchiveStage'
  ATTRS = ('GetVersion', 'WaitForBreakpadSymbols',)

  def GetVersion(self, inst):
    return 'R27-%s' % inst.release_tag if inst.release_tag else ''

  def WaitForBreakpadSymbols(self, _inst):
    return True


class ArchiveStageTest(AbstractStageTest):

  def _PatchDependencies(self):
    """Patch dependencies of ArchiveStage.PerformStage()."""
    to_patch = [
        (parallel, 'RunParallelSteps'), (commands, 'PushImages'),
        (commands, 'RemoveOldArchives'), (commands, 'UploadArchivedFile')]
    self.AutoPatch(to_patch)

  def setUp(self):
    self._build_config = self.build_config.copy()
    self._build_config['upload_symbols'] = True
    self._build_config['push_image'] = True

    self.archive_mock = ArchiveStageMock()
    self.StartPatcher(self.archive_mock)
    self._PatchDependencies()

  def ConstructStage(self):
    return stages.ArchiveStage(self.options, self._build_config,
                               self._current_board, '')

  def testArchive(self):
    """Simple did-it-run test."""
    # TODO(davidjames): Test the individual archive steps as well.
    self.RunStage()
    # pylint: disable=E1101
    self.assertEquals(
        commands.UploadArchivedFile.call_args[0][2:],
        ('LATEST-%s' % self.TARGET_MANIFEST_BRANCH, False))

  def testNoPushImagesForRemoteTrybot(self):
    """Test that remote trybot overrides work to disable push images."""
    argv = ['--remote-trybot', '-r', self.build_root, '--buildnumber=1234',
            'x86-mario-release']
    parser = cbuildbot._CreateParser()
    (self.options, _args) = cbuildbot._ParseCommandLine(parser, argv)
    test_config = config.config['x86-mario-release']
    self._build_config = config.OverrideConfigForTrybot(test_config,
                                                        self.options)
    self.RunStage()
    # pylint: disable=E1101
    self.assertEquals(commands.PushImages.call_count, 0)

  def testArchiveMetadataJson(self):
    """Test that the json metadata is built correctly"""
    # First run the code.
    stage = self.ConstructStage()
    stage.ArchiveMetadataJson()

    # Now check the results.
    json_file = stage._upload_queue.get()
    self.assertEquals(json_file, [constants.METADATA_JSON])
    json_file = os.path.join(stage.archive_path, json_file[0])
    json_data = json.loads(osutils.ReadFile(json_file))

    important_keys = (
        'boards',
        'bot-config',
        'cros-version',
        'metadata-version',
        'sdk-version',
        'toolchain-tuple',
        'toolchain-url',
    )
    for key in important_keys:
      self.assertTrue(key in json_data)

    self.assertEquals(json_data['boards'], ['x86-generic'])
    self.assertEquals(json_data['bot-config'], 'x86-generic-paladin')
    self.assertEquals(json_data['cros-version'], stage.version)
    self.assertEquals(json_data['metadata-version'], '1')

    # The buildtools manifest doesn't have any overlays. In this case, we can't
    # find any toolchains.
    overlays = portage_utilities.FindOverlays(constants.BOTH_OVERLAYS, None)
    overlay_tuples = ['i686-pc-linux-gnu', 'arm-none-eabi']
    self.assertEquals(json_data['toolchain-tuple'],
                      overlay_tuples if overlays else [])

  def testChromeEnvironment(self):
    """Test that the Chrome environment is built."""
    # Create the chrome environment compressed file.
    stage = self.ConstructStage()
    chrome_env_dir = os.path.join(
        stage._pkg_dir, constants.CHROME_CP + '-25.3643.0_rc1')
    env_file = os.path.join(chrome_env_dir, 'environment')
    osutils.Touch(env_file, makedirs=True)

    cros_build_lib.RunCommand(['bzip2', env_file])

    # Run the code.
    stage.ArchiveChromeEbuildEnv()

    env_tar = stage._upload_queue.get()[0]
    env_tar = os.path.join(stage.archive_path, env_tar)
    self.assertTrue(os.path.exists(env_tar))
    cros_test_lib.VerifyTarball(env_tar, ['./', 'environment'])


class UploadPrebuiltsStageTest(AbstractStageTest,
                               cros_build_lib_unittest.RunCommandTestCase):

  CMD = './upload_prebuilts'

  def setUp(self):
    self.options.prebuilts = True
    self.StartPatcher(ArchiveStageMock())
    self.archive_stage = stages.ArchiveStage(self.options, self.build_config,
                                             self._current_board, '')

  def ConstructStage(self):
    return stages.UploadPrebuiltsStage(self.options,
                                       self.build_config,
                                       self.build_config['boards'][-1],
                                       self.archive_stage)

  def VerifyBoardMap(self, bot_id, count, board_map, public_args=None,
                     private_args=None):
    """Verify that the prebuilts are uploaded for the specified bot.

    Arguments:
      bot_id: Bot to upload prebuilts for.
      count: Number of assert checks that should be performed.
      board_map: Map from slave boards to whether the bot is public.
      public_args: List of extra arguments for public boards.
      private_args: List of extra arguments for private boards.
    """
    self.build_config = copy.deepcopy(config.config[bot_id])
    self.RunStage()
    public_prefix = [self.CMD] + (public_args or [])
    private_prefix = [self.CMD] + (private_args or [])
    for board, public in board_map.iteritems():
      if public or public_args:
        public_cmd = public_prefix + ['--slave-board', board]
        self.assertCommandContains(public_cmd, expected=public)
        count -= 1
      private_cmd = private_prefix + ['--slave-board', board, '--private']
      self.assertCommandContains(private_cmd, expected=not public)
      count -= 1
    if board_map:
      self.assertCommandContains([self.CMD, '--set-version',
                                  self.archive_stage.version])
      count -= 1
    self.assertEqual(count, 0, 'Number of asserts performed does not match')

  def testFullPrebuiltsUpload(self):
    """Test uploading of full builder prebuilts."""
    self.VerifyBoardMap('x86-generic-full', 0, {})
    self.assertCommandContains([self.CMD, '--git-sync'])

  def testIncorrectCount(self):
    """Test that VerifyBoardMap asserts when the count is wrong."""
    self.assertRaises(AssertionError, self.VerifyBoardMap, 'x86-generic-full',
                      1, {})

  def testChromeUpload(self):
    """Test uploading of prebuilts for chrome build."""
    board_map = {'amd64-generic': True, 'daisy': True,
                 'x86-alex': False, 'lumpy': False}
    self.VerifyBoardMap('x86-generic-chromium-pfq', 9, board_map,
                        public_args=['--board', 'x86-generic'])

  def testPaladinMasterUpload(self):
    board_map = {'amd64-generic': True, 'x86-generic': True,
                 'x86-alex': False, 'lumpy': False, 'daisy_spring': False}
    self.VerifyBoardMap('mario-paladin', 8, board_map,
                        private_args=['--board', 'x86-mario'])
    self.assertCommandContains([self.CMD, '--sync-host'])


class UploadDevInstallerPrebuiltsStageTest(AbstractStageTest):
  def setUp(self):
    self.options.chrome_rev = None
    self.options.prebuilts = True
    self.build_config['dev_installer_prebuilts'] = True
    self.build_config['binhost_bucket'] = 'gs://testbucket'
    self.build_config['binhost_key'] = 'dontcare'
    self.build_config['binhost_base_url'] = 'https://dontcare/here'
    self.mox.StubOutWithMock(stages.UploadPrebuiltsStage, '_GetPortageEnvVar')
    self.mox.StubOutWithMock(commands, 'UploadDevInstallerPrebuilts')
    self.archive_stage = self.mox.CreateMock(stages.ArchiveStage)

  def ConstructStage(self):
    self.mox.CreateMock(stages.ArchiveStage)
    return stages.DevInstallerPrebuiltsStage(self.options,
                                             self.build_config,
                                             self._current_board,
                                             self.archive_stage)

  def testDevInstallerUpload(self):
    """Basic sanity test testing uploads of dev installer prebuilts."""
    version = 'awesome_canary_version'
    self.archive_stage.GetVersion().AndReturn(version)

    commands.UploadDevInstallerPrebuilts(
        binhost_bucket=self.build_config['binhost_bucket'],
        binhost_key=self.build_config['binhost_key'],
        binhost_base_url=self.build_config['binhost_base_url'],
        buildroot=self.build_root,
        board=self._current_board,
        extra_args=mox.And(mox.IsA(list),
                           mox.In(version)))

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class PublishUprevChangesStageTest(AbstractStageTest):

  def setUp(self):
    # Disable most paths by default and selectively enable in tests

    self.build_config['build_type'] = constants.BUILD_FROM_SOURCE_TYPE
    self.options.chrome_rev = constants.CHROME_REV_TOT
    self.options.prebuilts = True
    self.mox.StubOutWithMock(stages.PublishUprevChangesStage,
                             '_GetPortageEnvVar')
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')
    self.mox.StubOutWithMock(commands, 'UprevPush')

  def ConstructStage(self):
    return stages.PublishUprevChangesStage(self.options, self.build_config)

  def testPush(self):
    """Test values for PublishUprevChanges."""
    self.build_config['push_overlays'] = constants.PUBLIC_OVERLAYS
    self.build_config['master'] = True

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class PassStage(bs.BuilderStage):
  """PassStage always works"""


class Pass2Stage(bs.BuilderStage):
  """Pass2Stage always works"""


class FailStage(bs.BuilderStage):
  """FailStage always throws an exception"""

  FAIL_EXCEPTION = results_lib.StepFailure("Fail stage needs to fail.")

  def PerformStage(self):
    """Throw the exception to make us fail."""
    raise self.FAIL_EXCEPTION


class SkipStage(bs.BuilderStage):
  """SkipStage is skipped."""
  config_name = 'signer_tests'


class SneakyFailStage(bs.BuilderStage):
  """SneakyFailStage exits with an error."""

  def PerformStage(self):
    """Exit without reporting back."""
    os._exit(1)


class SuicideStage(bs.BuilderStage):
  """SuicideStage kills itself with kill -9."""

  def PerformStage(self):
    """Exit without reporting back."""
    os.kill(os.getpid(), signal.SIGKILL)


class BuildStagesResultsTest(cros_test_lib.TestCase):

  def setUp(self):
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-paladin'
    self.build_config = config.config[self.bot_id]
    self.build_root = '/fake_root'
    self.url = 'fake_url'

    # Create a class to hold
    class Options(object):
      pass

    self.options = Options()
    self.options.archive_base = 'gs://dontcare'
    self.options.buildroot = self.build_root
    self.options.debug = False
    self.options.prebuilts = False
    self.options.clobber = False
    self.options.nosdk = False
    self.options.latest_toolchain = False
    self.options.buildnumber = 1234
    self.options.chrome_rev = None
    results_lib.Results.Clear()

  def _runStages(self):
    """Run a couple of stages so we can capture the results"""
    # Run two pass stages, and one fail stage.
    PassStage(self.options, self.build_config).Run()
    Pass2Stage(self.options, self.build_config).Run()
    self.assertRaises(
      results_lib.StepFailure,
      FailStage(self.options, self.build_config).Run)

  def _verifyRunResults(self, expectedResults):

    actualResults = results_lib.Results.Get()

    # Break out the asserts to be per item to make debugging easier
    self.assertEqual(len(expectedResults), len(actualResults))
    for i in xrange(len(expectedResults)):
      name, result, description, runtime = actualResults[i]
      xname, xresult = expectedResults[i]

      if result not in results_lib.Results.NON_FAILURE_TYPES:
        self.assertTrue(isinstance(result, BaseException))
        if isinstance(result, results_lib.StepFailure):
          self.assertEqual(str(result), description)

      self.assertTrue(runtime >= 0 and runtime < 2.0)
      self.assertEqual(xname, name)
      self.assertEqual(type(xresult), type(result))
      self.assertEqual(repr(xresult), repr(result))

  def _PassString(self):
    return results_lib.Results.SPLIT_TOKEN.join(['Pass', 'None', '0\n'])

  def testRunStages(self):
    """Run some stages and verify the captured results"""

    self.assertEqual(results_lib.Results.Get(), [])

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', results_lib.Results.SUCCESS),
        ('Pass2', results_lib.Results.SUCCESS),
        ('Fail', FailStage.FAIL_EXCEPTION)]

    self._verifyRunResults(expectedResults)

  def testSuccessTest(self):
    """Run some stages and verify the captured results"""

    results_lib.Results.Record('Pass', results_lib.Results.SUCCESS)

    self.assertTrue(results_lib.Results.BuildSucceededSoFar())

    results_lib.Results.Record('Fail', FailStage.FAIL_EXCEPTION, time=1)

    self.assertFalse(results_lib.Results.BuildSucceededSoFar())

    results_lib.Results.Record('Pass2', results_lib.Results.SUCCESS)

    self.assertFalse(results_lib.Results.BuildSucceededSoFar())

  def testParallelStages(self):
    stage_objs = [stage(self.options, self.build_config) for stage in
                  (PassStage, SneakyFailStage, FailStage, SuicideStage,
                   Pass2Stage)]
    error = None
    with mock.patch.multiple(parallel._BackgroundTask, PRINT_INTERVAL=0.01):
      try:
        cbuildbot.SimpleBuilder._RunParallelStages(stage_objs)
      except parallel.BackgroundFailure as ex:
        error = ex
    self.assertTrue(error)
    expectedResults = [
        ('Pass', results_lib.Results.SUCCESS),
        ('Fail', FailStage.FAIL_EXCEPTION),
        ('Pass2', results_lib.Results.SUCCESS),
        ('SneakyFail', error),
        ('Suicide', error),
    ]
    self._verifyRunResults(expectedResults)

  def testStagesReportSuccess(self):
    """Tests Stage reporting."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    results_lib.Results.Record('Sync', results_lib.Results.SUCCESS, time=1)
    results_lib.Results.Record('Build', results_lib.Results.SUCCESS, time=2)
    results_lib.Results.Record('Test', FailStage.FAIL_EXCEPTION, time=3)
    results_lib.Results.Record('SignerTests', results_lib.Results.SKIPPED)
    result = cros_build_lib.CommandResult(cmd=['/bin/false', '/nosuchdir'],
                                          returncode=2)
    results_lib.Results.Record(
        'Archive',
        cros_build_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            result), time=4)

    results = StringIO.StringIO()

    results_lib.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Sync (0:00:01)\n"
        "************************************************************\n"
        "** PASS Build (0:00:02)\n"
        "************************************************************\n"
        "** FAIL Test (0:00:03) with StepFailure\n"
        "************************************************************\n"
        "** FAIL Archive (0:00:04) in /bin/false\n"
        "************************************************************\n"
    )

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(min(len(actualLines), len(expectedLines))):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testStagesReportError(self):
    """Tests Stage reporting with exceptions."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    results_lib.Results.Record('Sync', results_lib.Results.SUCCESS, time=1)
    results_lib.Results.Record('Build', results_lib.Results.SUCCESS, time=2)
    results_lib.Results.Record('Test', FailStage.FAIL_EXCEPTION,
                               'failException Msg\nLine 2', time=3)
    result = cros_build_lib.CommandResult(cmd=['/bin/false', '/nosuchdir'],
                                          returncode=2)
    results_lib.Results.Record(
        'Archive',
        cros_build_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            result),
        'FailRunCommand msg', time=4)

    results = StringIO.StringIO()

    results_lib.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Sync (0:00:01)\n"
        "************************************************************\n"
        "** PASS Build (0:00:02)\n"
        "************************************************************\n"
        "** FAIL Test (0:00:03) with StepFailure\n"
        "************************************************************\n"
        "** FAIL Archive (0:00:04) in /bin/false\n"
        "************************************************************\n"
        "\n"
        "Failed in stage Test:\n"
        "\n"
        "failException Msg\n"
        "Line 2\n"
        "\n"
        "Failed in stage Archive:\n"
        "\n"
        "FailRunCommand msg\n"
   )

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(min(len(actualLines), len(expectedLines))):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testStagesReportReleaseTag(self):
    """Tests Release Tag entry in stages report."""

    current_version = "release_tag_string"
    archive_urls = {'board1': 'result_url1',
                    'board2': 'result_url2'}

    # Store off a known set of results and generate a report
    results_lib.Results.Record('Pass', results_lib.Results.SUCCESS, time=1)

    results = StringIO.StringIO()

    results_lib.Results.Report(results, archive_urls, current_version)

    expectedResults = (
        "************************************************************\n"
        "** RELEASE VERSION: release_tag_string\n"
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Pass (0:00:01)\n"
        "************************************************************\n"
        "** BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:\n"
        "**  board1: result_url1\n"
        "@@@STEP_LINK@Artifacts[board1]@result_url1@@@\n"
        "**  board2: result_url2\n"
        "@@@STEP_LINK@Artifacts[board2]@result_url2@@@\n"
        "************************************************************\n")

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(len(expectedLines)):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testSaveCompletedStages(self):
    """Tests that we can save out completed stages."""

    # Run this again to make sure we have the expected results stored
    results_lib.Results.Record('Pass', results_lib.Results.SUCCESS)
    results_lib.Results.Record('Fail', FailStage.FAIL_EXCEPTION)
    results_lib.Results.Record('Pass2', results_lib.Results.SUCCESS)

    saveFile = StringIO.StringIO()
    results_lib.Results.SaveCompletedStages(saveFile)
    self.assertEqual(saveFile.getvalue(), self._PassString())

  def testRestoreCompletedStages(self):
    """Tests that we can read in completed stages."""

    results_lib.Results.RestoreCompletedStages(
        StringIO.StringIO(self._PassString()))

    previous = results_lib.Results.GetPrevious()
    self.assertEqual(previous.keys(), ['Pass'])

  def testRunAfterRestore(self):
    """Tests that we skip previously completed stages."""

    # Fake results_lib.Results.RestoreCompletedStages
    results_lib.Results.RestoreCompletedStages(
        StringIO.StringIO(self._PassString()))

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', results_lib.Results.SUCCESS),
        ('Pass2', results_lib.Results.SUCCESS),
        ('Fail', FailStage.FAIL_EXCEPTION)]

    self._verifyRunResults(expectedResults)


class ReportStageTest(AbstractStageTest):

  def setUp(self):
    for cmd in ((osutils, 'ReadFile'), (osutils, 'WriteFile'),
                (commands, 'UploadArchivedFile'),):
      self.StartPatcher(mock.patch.object(*cmd, autospec=True))
    self.StartPatcher(ArchiveStageMock())

  def ConstructStage(self):
    archive_stage = stages.ArchiveStage(self.options, self.build_config,
                                        self._current_board, '')
    archive_stages = {
        cbuildbot.BoardConfig('board', 'config1'): archive_stage,
        cbuildbot.BoardConfig('zororororor', 'config1'): archive_stage,
        cbuildbot.BoardConfig('mattress-man', 'config2'): archive_stage,
    }
    return stages.ReportStage(self.options, self.build_config,
                              archive_stages, None)

  def testCheckResults(self):
    """Basic sanity check for results stage functionality"""
    self.RunStage()


class BoardSpecificBuilderStageTest(cros_test_lib.TestCase):

  def testCheckOptions(self):
    """Makes sure options/config settings are setup correctly."""

    parser = cbuildbot._CreateParser()
    (options, _) = parser.parse_args([])

    for attr in dir(stages):
      obj = eval('stages.' + attr)
      if not hasattr(obj, '__base__'):
        continue
      if not obj.__base__ is stages.BoardSpecificBuilderStage:
        continue
      if obj.option_name:
        self.assertTrue(getattr(options, obj.option_name))
      if obj.config_name:
        if not obj.config_name in config._settings:
          self.fail(('cbuildbot_stages.%s.config_name "%s" is missing from '
                     'cbuildbot_config._settings') % (attr, obj.config_name))


class MockPatch(mock.MagicMock):
  gerrit_number = '1234'
  patch_number = '1'
  project = 'chromiumos/chromite'


class BaseCQTest(StageTest):
  """Helper class for testing the CommitQueueSync stage"""
  MANIFEST_CONTENTS = '<manifest/>'
  PALADIN_BOT_ID = None

  def setUp(self):
    """Setup patchers for specified bot id."""
    self.build_config = copy.deepcopy(config.config[self.PALADIN_BOT_ID])
    self.sync_stage = stages.CommitQueueSyncStage(self.options,
                                                  self.build_config)
    # Mock out methods as needed.
    self.AutoPatch([[gerrit.GerritHelper, '_SqlQuery'],
                    [lkgm_manager, 'GenerateBlameList']])
    self.PatchObject(repository.RepoRepository, 'ExportManifest',
                     return_value=self.MANIFEST_CONTENTS, autospec=True)
    self.PatchObject(validation_pool.ValidationPool, 'MAX_TIMEOUT', 0.5)
    self.StartPatcher(git_unittest.ManifestMock())
    self.StartPatcher(git_unittest.ManifestCheckoutMock())
    version_file = os.path.join(self.build_root, constants.VERSION_FILE)
    manifest_version_unittest.VersionInfoTest.WriteFakeVersionFile(version_file)
    rc_mock = self.StartPatcher(cros_build_lib_unittest.RunCommandMock())
    rc_mock.SetDefaultCmdResult()

    # Create a fake repo / manifest on disk that is used by subclasses.
    for subdir in ('repo', 'manifests'):
      osutils.SafeMakedirs(os.path.join(self.build_root, '.repo', subdir))
    self.manifest_path = os.path.join(self.build_root, '.repo', 'manifest.xml')
    osutils.WriteFile(self.manifest_path, self.MANIFEST_CONTENTS)

  def PerformSync(self, remote='cros', committed=True, tree_open=True,
                  tracking_branch='master', num_patches=1):
    """Helper to perform a basic sync for master commit queue."""
    p = MockPatch(remote=remote, tracking_branch=tracking_branch)
    my_patches = [p] * num_patches
    self.PatchObject(gerrit.GerritHelper, 'IsChangeCommitted',
                     return_value=committed, autospec=True)
    self.PatchObject(gerrit.GerritHelper, 'Query', return_value=my_patches,
                     autospec=True)
    self.PatchObject(cros_build_lib, 'TreeOpen', return_value=tree_open,
                     autospec=True)
    self.sync_stage.PerformStage()

  def ReloadPool(self):
    """Save the pool to disk and reload it."""
    with tempfile.NamedTemporaryFile() as f:
      cPickle.dump(self.sync_stage.pool, f)
      f.flush()
      self.options.validation_pool = f.name
      self.sync_stage = stages.CommitQueueSyncStage(self.options,
                                                    self.build_config)
      self.sync_stage.HandleSkip()


class SlaveCQSyncTest(BaseCQTest):
  """Tests the CommitQueueSync stage for the paladin slaves."""
  PALADIN_BOT_ID = 'alex-paladin'

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    self.PatchObject(lkgm_manager.LKGMManager, 'GetLatestCandidate',
                     return_value=self.manifest_path, autospec=True)
    self.sync_stage.PerformStage()
    self.ReloadPool()


class MasterCQSyncTest(BaseCQTest):
  """Tests the CommitQueueSync stage for the paladin masters.

  Tests in this class should apply both to the paladin masters and to the
  Pre-CQ Launcher.
  """
  PALADIN_BOT_ID = 'mario-paladin'

  def setUp(self):
    """Setup patchers for specified bot id."""
    self.AutoPatch([[validation_pool.ValidationPool, 'ApplyPoolIntoRepo']])
    self.PatchObject(lkgm_manager.LKGMManager, 'CreateNewCandidate',
                     return_value=self.manifest_path, autospec=True)

  def testCommitNonManifestChange(self, **kwargs):
    """Test the commit of a non-manifest change."""
    # Setting tracking_branch=foo makes this a non-manifest change.
    self.PerformSync(tracking_branch='foo', **kwargs)

  def testFailedCommitOfNonManifestChange(self):
    """Test that the commit of a non-manifest change fails."""
    self.testCommitNonManifestChange(committed=False)

  def testDefaultSync(self):
    """Test basic ability to sync with standard options."""
    self.PerformSync()

  def testNoGerritHelper(self):
    """Test that setting a non-standard remote raises an exception."""
    self.assertRaises(validation_pool.GerritHelperNotAvailable,
                      self.testCommitNonManifestChange, remote='foo')


class ExtendedMasterCQSyncTest(MasterCQSyncTest):
  """Additional tests for the CommitQueueSync stage.

  These only apply to the paladin master and not to any other stages.
  """

  def testReload(self):
    """Test basic ability to sync and reload the patches from disk."""
    # Use zero patches because MockPatches can't be pickled. Also set debug mode
    # so that the CQ won't wait for more patches.
    self.options.debug = True
    self.PerformSync(num_patches=0)
    self.ReloadPool()

  def testTreeClosureBlocksCommit(self):
    """Test that tree closures block commits."""
    self.assertRaises(SystemExit, self.testCommitNonManifestChange,
                      tree_open=False)


class PreCQLauncherStageTest(MasterCQSyncTest):
  """Tests for the PreCQLauncherStage."""
  PALADIN_BOT_ID = 'pre-cq-launcher'

  def setUp(self):
    old_sleep = time.sleep
    self.PatchObject(time, 'sleep', side_effect=lambda x: old_sleep(0.1))
    self.sync_stage = stages.PreCQLauncherStage(self.options, self.build_config)

  def testTreeClosureIsOK(self):
    """Test that tree closures block commits."""
    self.testCommitNonManifestChange(tree_open=False)


if __name__ == '__main__':
  cros_test_lib.main()
