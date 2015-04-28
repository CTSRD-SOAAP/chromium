# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import inspect
import os

from telemetry import user_story as user_story_module
from telemetry.wpr import archive_info


class UserStorySet(object):
  """A collection of user story.

  A typical usage of UserStorySet would be to subclass it and then calling
  AddUserStory for each UserStory..
  """

  def __init__(self, archive_data_file='', cloud_storage_bucket=None,
               serving_dirs=None):
    """Creates a new UserStorySet.

    Args:
      archive_data_file: The path to Web Page Replay's archive data, relative
          to self.base_dir.
      cloud_storage_bucket: The cloud storage bucket used to download
          Web Page Replay's archive data. Valid values are: None,
          PUBLIC_BUCKET, PARTNER_BUCKET, or INTERNAL_BUCKET (defined
          in telemetry.util.cloud_storage).
    """
    self.user_stories = []
    self._archive_data_file = archive_data_file
    self._wpr_archive_info = None
    archive_info.AssertValidCloudStorageBucket(cloud_storage_bucket)
    self._cloud_storage_bucket = cloud_storage_bucket
    self._base_dir = os.path.dirname(inspect.getfile(self.__class__))
    # Convert any relative serving_dirs to absolute paths.
    self._serving_dirs = set(os.path.realpath(os.path.join(self.base_dir, d))
                             for d in serving_dirs or [])

  @property
  def base_dir(self):
    """The base directory to resolve archive_data_file.

    This defaults to the directory containing the UserStorySet instance's class.
    """
    return self._base_dir

  @property
  def serving_dirs(self):
    return self._serving_dirs

  @property
  def archive_data_file(self):
    return self._archive_data_file

  @property
  def bucket(self):
    return self._cloud_storage_bucket

  @property
  def wpr_archive_info(self):
    """Lazily constructs wpr_archive_info if it's not set and returns it."""
    if self.archive_data_file and not self._wpr_archive_info:
      self._wpr_archive_info = archive_info.WprArchiveInfo.FromFile(
          os.path.join(self.base_dir, self.archive_data_file), self.bucket)
    return self._wpr_archive_info

  def AddUserStory(self, user_story):
    assert isinstance(user_story, user_story_module.UserStory)
    self.user_stories.append(user_story)

  @classmethod
  def Name(cls):
    """ Returns the string name of this UserStorySet.
    Note that this should be a classmethod so benchmark_runner script can match
    user story class with its name specified in the run command:
    'Run <User story test name> <User story class name>'
    """
    return cls.__module__.split('.')[-1]

  @classmethod
  def Description(cls):
    """ Return a string explaining in human-understandable terms what this
    user story represents.
    Note that this should be a classmethod so benchmark_runner script can
    display user stories' names along their descriptions in the list commmand.
    """
    if cls.__doc__:
      return cls.__doc__.splitlines()[0]
    else:
      return ''

  def ShuffleAndFilterUserStorySet(self, finder_options):
    pass

  def WprFilePathForUserStory(self, story):
    """Convenient function to retrive WPR archive file path.

    Args:
      user_story: The UserStory to lookup.

    Returns:
      The WPR archive file path for the given UserStory, if found.
      Otherwise, return None.
    """
    if not self.wpr_archive_info:
      return None
    return self.wpr_archive_info.WprFilePathForUserStory(story)

  def __iter__(self):
    return self.user_stories.__iter__()

  def __len__(self):
    return len(self.user_stories)

  def __getitem__(self, key):
    return self.user_stories[key]

  def __setitem__(self, key, value):
    self.user_stories[key] = value
