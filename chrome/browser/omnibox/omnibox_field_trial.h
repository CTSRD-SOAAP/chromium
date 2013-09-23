// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_OMNIBOX_FIELD_TRIAL_H_
#define CHROME_BROWSER_OMNIBOX_OMNIBOX_FIELD_TRIAL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

// This class manages the Omnibox field trials.
class OmniboxFieldTrial {
 public:
  // Creates the static field trial groups.
  // *** MUST NOT BE CALLED MORE THAN ONCE. ***
  static void ActivateStaticTrials();

  // Activates all dynamic field trials.  The main difference between
  // the autocomplete dynamic and static field trials is that the former
  // don't require any code changes on the Chrome side as they are controlled
  // on the server side.  Chrome binary simply propagates all necessary
  // information through the X-Chrome-Variations header.
  // This method, unlike ActivateStaticTrials(), may be called multiple times.
  static void ActivateDynamicTrials();

  // Returns a bitmap containing AutocompleteProvider::Type values
  // that should be disabled in AutocompleteController.
  // This method simply goes over all autocomplete dynamic field trial groups
  // and looks for group names like "ProvidersDisabled_NNN" where NNN is
  // an integer corresponding to a bitmap mask.  All extracted bitmaps
  // are OR-ed together and returned as the final result.
  static int GetDisabledProviderTypes();

  // ---------------------------------------------------------
  // For the inline History Quick Provider field trial.

  // Returns whether the user is in any field trial group for this
  // field trial.  False indicates that the field trial wasn't
  // successfully created for some reason.
  static bool InDisallowInlineHQPFieldTrial();

  // Returns whether the user should get the experiment setup or
  // the default setup for this field trial.  The experiment
  // group prohibits inlining suggestions.
  static bool InDisallowInlineHQPFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the suggest field trial.

  // Populates |field_trial_hash| with hashes of the active suggest field trial
  // names, if any.
  static void GetActiveSuggestFieldTrialHashes(
      std::vector<uint32>* field_trial_hash);

  // ---------------------------------------------------------
  // For the HistoryURL provider disable culling redirects field trial.

  // Returns whether the user is in any group for this field trial.
  // (Should always be true unless initialization went wrong.)
  static bool InHUPCullRedirectsFieldTrial();

  // Returns whether we should disable culling of redirects in
  // HistoryURL provider.
  static bool InHUPCullRedirectsFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the HistoryURL provider disable creating a shorter match
  // field trial.

  // Returns whether the user is in any group for this field trial.
  // (Should always be true unless initialization went wrong.)
  static bool InHUPCreateShorterMatchFieldTrial();

  // Returns whether we should disable creating a shorter match in
  // HistoryURL provider.
  static bool InHUPCreateShorterMatchFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the AutocompleteController "stop timer" field trial.

  // Returns whether the user should get the experimental setup or the
  // default setup for this field trial.  The experiment group uses
  // a timer in AutocompleteController to tell the providers to stop
  // looking for matches after too much time has passed.  In other words,
  // it tries to tell the providers to stop updating the list of suggested
  // matches if updating the matches would probably be disruptive because
  // they're arriving so late.
  static bool InStopTimerFieldTrialExperimentGroup();

  // ---------------------------------------------------------
  // For the ZeroSuggestProvider field trial.

  // Returns whether the user is in any field trial where the
  // ZeroSuggestProvider should be used to get suggestions when the
  // user clicks on the omnibox but has not typed anything yet.
  static bool InZeroSuggestFieldTrial();

  // ---------------------------------------------------------
  // For the ShortcutsScoring field trial.

  // If the field trial is active and the user is in an experiment
  // group, extract from the experiment group name the maximum
  // relevance score ShortcutsProvider:: CalculateScore() can return.
  // Returns true on a successful extraction.  If the extraction failed,
  // if the field trial is not active, etc., returns false.
  // CalculateScore()'s return value is a product of this maximum
  // relevance score and some attenuating factors that are all between
  // 0 and 1.  (Note that Shortcuts results may have their scores
  // reduced later if the assigned score is higher than allowed for
  // non-inlineable results.  Shortcuts results are not allowed to be
  // inlined.)
  static bool ShortcutsScoringMaxRelevance(int* max_relevance);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OmniboxFieldTrial);
};

#endif  // CHROME_BROWSER_OMNIBOX_OMNIBOX_FIELD_TRIAL_H_
