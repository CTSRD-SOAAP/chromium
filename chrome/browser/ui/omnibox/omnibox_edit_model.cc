// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"

#include <string>

#include "base/auto_reset.h"
#include "base/format_macros.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/extension_app_provider.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/omnibox/omnibox_log.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/omnibox/omnibox_current_page_delegate_impl.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/common/constants.h"
#include "googleurl/src/url_util.h"
#include "ui/gfx/image/image.h"

using content::UserMetricsAction;
using predictors::AutocompleteActionPredictor;
using predictors::AutocompleteActionPredictorFactory;

namespace {

// Histogram name which counts the number of times that the user text is
// cleared.  IME users are sometimes in the situation that IME was
// unintentionally turned on and failed to input latin alphabets (ASCII
// characters) or the opposite case.  In that case, users may delete all
// the text and the user text gets cleared.  We'd like to measure how often
// this scenario happens.
//
// Note that since we don't currently correlate "text cleared" events with
// IME usage, this also captures many other cases where users clear the text;
// though it explicitly doesn't log deleting all the permanent text as
// the first action of an editing sequence (see comments in
// OnAfterPossibleChange()).
const char kOmniboxUserTextClearedHistogram[] = "Omnibox.UserTextCleared";

enum UserTextClearedType {
  OMNIBOX_USER_TEXT_CLEARED_BY_EDITING = 0,
  OMNIBOX_USER_TEXT_CLEARED_WITH_ESCAPE = 1,
  OMNIBOX_USER_TEXT_CLEARED_NUM_OF_ITEMS,
};

// Histogram name which counts the number of times the user enters
// keyword hint mode and via what method.  The possible values are listed
// in the EnteredKeywordModeMethod enum which is defined in the .h file.
const char kEnteredKeywordModeHistogram[] = "Omnibox.EnteredKeywordMode";

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// OmniboxEditModel::State

OmniboxEditModel::State::State(bool user_input_in_progress,
                               const string16& user_text,
                               const string16& instant_suggestion,
                               const string16& keyword,
                               bool is_keyword_hint,
                               OmniboxFocusState focus_state)
    : user_input_in_progress(user_input_in_progress),
      user_text(user_text),
      instant_suggestion(instant_suggestion),
      keyword(keyword),
      is_keyword_hint(is_keyword_hint),
      focus_state(focus_state) {
}

OmniboxEditModel::State::~State() {
}

///////////////////////////////////////////////////////////////////////////////
// OmniboxEditModel

OmniboxEditModel::OmniboxEditModel(OmniboxView* view,
                                   OmniboxEditController* controller,
                                   Profile* profile)
    : view_(view),
      controller_(controller),
      focus_state_(OMNIBOX_FOCUS_NONE),
      user_input_in_progress_(false),
      just_deleted_text_(false),
      has_temporary_text_(false),
      is_temporary_text_set_by_instant_(false),
      selected_instant_autocomplete_match_index_(OmniboxPopupModel::kNoMatch),
      is_instant_temporary_text_a_search_query_(false),
      paste_state_(NONE),
      control_key_state_(UP),
      is_keyword_hint_(false),
      profile_(profile),
      in_revert_(false),
      in_escape_handler_(false),
      allow_exact_keyword_match_(false) {
  omnibox_controller_.reset(new OmniboxController(this, profile));
  delegate_.reset(new OmniboxCurrentPageDelegateImpl(controller, profile));
}

OmniboxEditModel::~OmniboxEditModel() {
}

const OmniboxEditModel::State OmniboxEditModel::GetStateForTabSwitch() {
  // Like typing, switching tabs "accepts" the temporary text as the user
  // text, because it makes little sense to have temporary text when the
  // popup is closed.
  if (user_input_in_progress_) {
    // Weird edge case to match other browsers: if the edit is empty, revert to
    // the permanent text (so the user can get it back easily) but select it (so
    // on switching back, typing will "just work").
    const string16 user_text(UserTextFromDisplayText(view_->GetText()));
    if (user_text.empty()) {
      base::AutoReset<bool> tmp(&in_revert_, true);
      view_->RevertAll();
      view_->SelectAll(true);
    } else {
      InternalSetUserText(user_text);
    }
  }

  return State(user_input_in_progress_,
               user_text_,
               view_->GetInstantSuggestion(),
               keyword_,
               is_keyword_hint_,
               focus_state_);
}

void OmniboxEditModel::RestoreState(const State& state) {
  SetFocusState(state.focus_state, OMNIBOX_FOCUS_CHANGE_TAB_SWITCH);
  // Restore any user editing.
  if (state.user_input_in_progress) {
    // NOTE: Be sure and set keyword-related state BEFORE invoking
    // DisplayTextFromUserText(), as its result depends upon this state.
    keyword_ = state.keyword;
    is_keyword_hint_ = state.is_keyword_hint;
    view_->SetUserText(state.user_text,
        DisplayTextFromUserText(state.user_text), false);
    view_->SetInstantSuggestion(state.instant_suggestion);
  }
}

AutocompleteMatch OmniboxEditModel::CurrentMatch(
    GURL* alternate_nav_url) const {
  // If we have a valid match use it. Otherwise get one for the current text.
  AutocompleteMatch match = omnibox_controller_->current_match();

  if (!match.destination_url.is_valid()) {
    GetInfoForCurrentText(&match, alternate_nav_url);
  } else if (alternate_nav_url) {
    *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        autocomplete_controller()->input(), match);
  }
  return match;
}

bool OmniboxEditModel::UpdatePermanentText(const string16& new_permanent_text) {
  // When there's a new URL, and the user is not editing anything or the edit
  // doesn't have focus, we want to revert the edit to show the new URL.  (The
  // common case where the edit doesn't have focus is when the user has started
  // an edit and then abandoned it and clicked a link on the page.)
  //
  // If the page is auto-committing an instant suggestion, however, we generally
  // don't want to make any change to the edit.  While auto-commits modify the
  // underlying permanent URL, they're intended to have no effect on the user's
  // editing process -- before and after the auto-commit, the omnibox should
  // show the same user text and the same instant suggestion, even if the
  // auto-commit happens while the edit doesn't have focus.
  string16 instant_suggestion = view_->GetInstantSuggestion();
  const bool visibly_changed_permanent_text =
      (permanent_text_ != new_permanent_text) &&
      (!has_focus() ||
       (!user_input_in_progress_ && !popup_model()->IsOpen())) &&
      (instant_suggestion.empty() ||
       new_permanent_text != user_text_ + instant_suggestion);

  permanent_text_ = new_permanent_text;
  return visibly_changed_permanent_text;
}

GURL OmniboxEditModel::PermanentURL() {
  return URLFixerUpper::FixupURL(UTF16ToUTF8(permanent_text_), std::string());
}

void OmniboxEditModel::SetUserText(const string16& text) {
  SetInputInProgress(true);
  InternalSetUserText(text);
  omnibox_controller_->InvalidateCurrentMatch();
  paste_state_ = NONE;
  has_temporary_text_ = false;
  is_temporary_text_set_by_instant_ = false;
  selected_instant_autocomplete_match_index_ = OmniboxPopupModel::kNoMatch;
  is_instant_temporary_text_a_search_query_ = false;
}

void OmniboxEditModel::SetInstantSuggestion(
    const InstantSuggestion& suggestion) {
// Should only get called for the HTML popup.
#if defined(HTML_INSTANT_EXTENDED_POPUP)
  omnibox_controller_->SetInstantSuggestion(suggestion);
#endif
}

bool OmniboxEditModel::CommitSuggestedText() {
  const string16 suggestion = view_->GetInstantSuggestion();
  if (suggestion.empty())
    return false;

  // Assume that the gray text we are committing is a search suggestion.
  const string16 final_text = view_->GetText() + suggestion;
  view_->OnBeforePossibleChange();
  view_->SetWindowTextAndCaretPos(final_text, final_text.length(), false,
      false);
  view_->OnAfterPossibleChange();
  return true;
}

void OmniboxEditModel::OnChanged() {
  // Don't call CurrentMatch() when there's no editing, as in this case we'll
  // never actually use it.  This avoids running the autocomplete providers (and
  // any systems they then spin up) during startup.
  const AutocompleteMatch& current_match = user_input_in_progress_ ?
      CurrentMatch(NULL) : AutocompleteMatch();

  AutocompleteActionPredictor::Action recommended_action =
      AutocompleteActionPredictor::ACTION_NONE;
  AutocompleteActionPredictor* action_predictor =
      user_input_in_progress_ ?
      AutocompleteActionPredictorFactory::GetForProfile(profile_) : NULL;
  if (action_predictor) {
    action_predictor->RegisterTransitionalMatches(user_text_, result());
    // Confer with the AutocompleteActionPredictor to determine what action, if
    // any, we should take. Get the recommended action here even if we don't
    // need it so we can get stats for anyone who is opted in to UMA, but only
    // get it if the user has actually typed something to avoid constructing it
    // before it's needed. Note: This event is triggered as part of startup when
    // the initial tab transitions to the start page.
    recommended_action =
        action_predictor->RecommendAction(user_text_, current_match);
  }

  UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.Action",
                            recommended_action,
                            AutocompleteActionPredictor::LAST_PREDICT_ACTION);

  // Do not perform instant if we're currently reverting or the change is the
  // result of an INSTANT_COMPLETE_REPLACE instant suggestion.
  bool performed_instant = false;
  if (!in_revert_ && !is_temporary_text_set_by_instant_) {
    size_t start, end;
    view_->GetSelectionBounds(&start, &end);
    string16 user_text = DisplayTextFromUserText(user_text_);
    performed_instant = omnibox_controller_->DoInstant(
        current_match, user_text, view_->GetText(), start, end,
        user_input_in_progress_, in_escape_handler_,
        view_->DeleteAtEndPressed() || just_deleted_text_,
        KeywordIsSelected());
  }

  if (!performed_instant) {
    // Hide any suggestions we might be showing.
    view_->SetInstantSuggestion(string16());

    // No need to wait any longer for Instant.
    omnibox_controller_->FinalizeInstantQuery(string16(), InstantSuggestion());
  }

  switch (recommended_action) {
    case AutocompleteActionPredictor::ACTION_PRERENDER:
      // It's possible that there is no current page, for instance if the tab
      // has been closed or on return from a sleep state.
      // (http://crbug.com/105689)
      if (!delegate_->CurrentPageExists())
        break;
      // Ask for prerendering if the destination URL is different than the
      // current URL.
      if (current_match.destination_url != PermanentURL())
        delegate_->DoPrerender(current_match);
      break;
    case AutocompleteActionPredictor::ACTION_PRECONNECT:
      omnibox_controller_->DoPreconnect(current_match);
      break;
    case AutocompleteActionPredictor::ACTION_NONE:
      break;
  }

  controller_->OnChanged();
}

void OmniboxEditModel::GetDataForURLExport(GURL* url,
                                           string16* title,
                                           gfx::Image* favicon) {
  *url = CurrentMatch(NULL).destination_url;
  if (*url == URLFixerUpper::FixupURL(UTF16ToUTF8(permanent_text_),
                                      std::string())) {
    *title = controller_->GetTitle();
    *favicon = controller_->GetFavicon();
  }
}

bool OmniboxEditModel::CurrentTextIsURL() const {
  if (view_->toolbar_model()->WouldReplaceSearchURLWithSearchTerms())
    return false;

  // If current text is not composed of replaced search terms and
  // !user_input_in_progress_, then permanent text is showing and should be a
  // URL, so no further checking is needed.  By avoiding checking in this case,
  // we avoid calling into the autocomplete providers, and thus initializing the
  // history system, as long as possible, which speeds startup.
  if (!user_input_in_progress_)
    return true;

  return !AutocompleteMatch::IsSearchType(CurrentMatch(NULL).type);
}

AutocompleteMatch::Type OmniboxEditModel::CurrentTextType() const {
  return CurrentMatch(NULL).type;
}

void OmniboxEditModel::AdjustTextForCopy(int sel_min,
                                         bool is_all_selected,
                                         string16* text,
                                         GURL* url,
                                         bool* write_url) {
  *write_url = false;

  // Do not adjust if selection did not start at the beginning of the field, or
  // if the URL was replaced by search terms.
  if ((sel_min != 0) ||
      view_->toolbar_model()->WouldReplaceSearchURLWithSearchTerms())
    return;

  if (!user_input_in_progress_ && is_all_selected) {
    // The user selected all the text and has not edited it. Use the url as the
    // text so that if the scheme was stripped it's added back, and the url
    // is unescaped (we escape parts of the url for display).
    *url = PermanentURL();
    *text = UTF8ToUTF16(url->spec());
    *write_url = true;
    return;
  }

  // We can't use CurrentTextIsURL() or GetDataForURLExport() because right now
  // the user is probably holding down control to cause the copy, which will
  // screw up our calculation of the desired_tld.
  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(profile_)->Classify(*text,
      KeywordIsSelected(), true, &match, NULL);
  if (AutocompleteMatch::IsSearchType(match.type))
    return;
  *url = match.destination_url;

  // Prefix the text with 'http://' if the text doesn't start with 'http://',
  // the text parses as a url with a scheme of http, the user selected the
  // entire host, and the user hasn't edited the host or manually removed the
  // scheme.
  GURL perm_url(PermanentURL());
  if (perm_url.SchemeIs(chrome::kHttpScheme) &&
      url->SchemeIs(chrome::kHttpScheme) && perm_url.host() == url->host()) {
    *write_url = true;
    string16 http = ASCIIToUTF16(chrome::kHttpScheme) +
        ASCIIToUTF16(content::kStandardSchemeSeparator);
    if (text->compare(0, http.length(), http) != 0)
      *text = http + *text;
  }
}

void OmniboxEditModel::SetInputInProgress(bool in_progress) {
  if (user_input_in_progress_ == in_progress)
    return;

  user_input_in_progress_ = in_progress;
  if (user_input_in_progress_) {
    time_user_first_modified_omnibox_ = base::TimeTicks::Now();
    content::RecordAction(content::UserMetricsAction("OmniboxInputInProgress"));
    autocomplete_controller()->ResetSession();
  }
  controller_->OnInputInProgress(in_progress);

  delegate_->NotifySearchTabHelper(user_input_in_progress_, !in_revert_,
                                   popup_model()->IsOpen(), user_text_.empty());
}

void OmniboxEditModel::Revert() {
  SetInputInProgress(false);
  paste_state_ = NONE;
  InternalSetUserText(string16());
  keyword_.clear();
  is_keyword_hint_ = false;
  has_temporary_text_ = false;
  is_temporary_text_set_by_instant_ = false;
  selected_instant_autocomplete_match_index_ = OmniboxPopupModel::kNoMatch;
  is_instant_temporary_text_a_search_query_ = false;
  view_->SetWindowTextAndCaretPos(permanent_text_,
                                  has_focus() ? permanent_text_.length() : 0,
                                  false, true);
  AutocompleteActionPredictor* action_predictor =
      AutocompleteActionPredictorFactory::GetForProfile(profile_);
  if (action_predictor)
    action_predictor->ClearTransitionalMatches();
}

void OmniboxEditModel::StartAutocomplete(
    bool has_selected_text,
    bool prevent_inline_autocomplete) const {
  size_t cursor_position;
  if (inline_autocomplete_text_.empty()) {
    // Cursor position is equivalent to the current selection's end.
    size_t start;
    view_->GetSelectionBounds(&start, &cursor_position);
    // Adjust cursor position taking into account possible keyword in the user
    // text.  We rely on DisplayTextFromUserText() method which is consistent
    // with keyword extraction done in KeywordProvider/SearchProvider.
    const size_t cursor_offset =
        user_text_.length() - DisplayTextFromUserText(user_text_).length();
    cursor_position += cursor_offset;
  } else {
    // There are some cases where StartAutocomplete() may be called
    // with non-empty |inline_autocomplete_text_|.  In such cases, we cannot
    // use the current selection, because it could result with the cursor
    // position past the last character from the user text.  Instead,
    // we assume that the cursor is simply at the end of input.
    // One example is when user presses Ctrl key while having a highlighted
    // inline autocomplete text.
    // TODO: Rethink how we are going to handle this case to avoid
    // inconsistent behavior when user presses Ctrl key.
    // See http://crbug.com/165961 and http://crbug.com/165968 for more details.
    cursor_position = user_text_.length();
  }

  GURL current_url =
      (delegate_->CurrentPageExists() && view_->IsIndicatingQueryRefinement()) ?
      delegate_->GetURL() : GURL();
  bool keyword_is_selected = KeywordIsSelected();
  omnibox_controller_->StartAutocomplete(
      user_text_,
      cursor_position,
      current_url,
      prevent_inline_autocomplete || just_deleted_text_ ||
      (has_selected_text && inline_autocomplete_text_.empty()) ||
      (paste_state_ != NONE),
      keyword_is_selected,
      keyword_is_selected || allow_exact_keyword_match_,
      controller_->GetOmniboxBounds().x());
}

void OmniboxEditModel::StopAutocomplete() {
  autocomplete_controller()->Stop(true);
}

bool OmniboxEditModel::CanPasteAndGo(const string16& text) const {
  if (!view_->command_updater()->IsCommandEnabled(IDC_OPEN_CURRENT_URL))
    return false;

  AutocompleteMatch match;
  ClassifyStringForPasteAndGo(text, &match, NULL);
  return match.destination_url.is_valid();
}

void OmniboxEditModel::PasteAndGo(const string16& text) {
  DCHECK(CanPasteAndGo(text));
  view_->RevertAll();
  AutocompleteMatch match;
  GURL alternate_nav_url;
  ClassifyStringForPasteAndGo(text, &match, &alternate_nav_url);
  view_->OpenMatch(match, CURRENT_TAB, alternate_nav_url,
                   OmniboxPopupModel::kNoMatch);
}

bool OmniboxEditModel::IsPasteAndSearch(const string16& text) const {
  AutocompleteMatch match;
  ClassifyStringForPasteAndGo(text, &match, NULL);
  return AutocompleteMatch::IsSearchType(match.type);
}

void OmniboxEditModel::AcceptInput(WindowOpenDisposition disposition,
                                   bool for_drop) {
  // Get the URL and transition type for the selected entry.
  GURL alternate_nav_url;
  AutocompleteMatch match = CurrentMatch(&alternate_nav_url);

  // If CTRL is down it means the user wants to append ".com" to the text he
  // typed. If we can successfully generate a URL_WHAT_YOU_TYPED match doing
  // that, then we use this. These matches are marked as generated by the
  // HistoryURLProvider so we only generate them if this provider is present.
  if (control_key_state_ == DOWN_WITHOUT_CHANGE && !KeywordIsSelected() &&
      autocomplete_controller()->history_url_provider()) {
    // Generate a new AutocompleteInput, copying the latest one but using "com"
    // as the desired TLD. Then use this autocomplete input to generate a
    // URL_WHAT_YOU_TYPED AutocompleteMatch. Note that using the most recent
    // input instead of the currently visible text means we'll ignore any
    // visible inline autocompletion: if a user types "foo" and is autocompleted
    // to "foodnetwork.com", ctrl-enter will  navigate to "foo.com", not
    // "foodnetwork.com".  At the time of writing, this behavior matches
    // Internet Explorer, but not Firefox.
    const AutocompleteInput& old_input = autocomplete_controller()->input();
    AutocompleteInput input(
      old_input.text(), old_input.cursor_position(), ASCIIToUTF16("com"),
      GURL(), old_input.prevent_inline_autocomplete(),
      old_input.prefer_keyword(), old_input.allow_exact_keyword_match(),
      old_input.matches_requested());
    AutocompleteMatch url_match = HistoryURLProvider::SuggestExactInput(
        autocomplete_controller()->history_url_provider(), input, true);

    if (url_match.destination_url.is_valid()) {
      // We have a valid URL, we use this newly generated AutocompleteMatch.
      match = url_match;
      alternate_nav_url = GURL();
    }
  }

  if (!match.destination_url.is_valid())
    return;

  if ((match.transition == content::PAGE_TRANSITION_TYPED) &&
      (match.destination_url ==
       URLFixerUpper::FixupURL(UTF16ToUTF8(permanent_text_), std::string()))) {
    // When the user hit enter on the existing permanent URL, treat it like a
    // reload for scoring purposes.  We could detect this by just checking
    // user_input_in_progress_, but it seems better to treat "edits" that end
    // up leaving the URL unchanged (e.g. deleting the last character and then
    // retyping it) as reloads too.  We exclude non-TYPED transitions because if
    // the transition is GENERATED, the user input something that looked
    // different from the current URL, even if it wound up at the same place
    // (e.g. manually retyping the same search query), and it seems wrong to
    // treat this as a reload.
    match.transition = content::PAGE_TRANSITION_RELOAD;
  } else if (for_drop || ((paste_state_ != NONE) &&
                          match.is_history_what_you_typed_match)) {
    // When the user pasted in a URL and hit enter, score it like a link click
    // rather than a normal typed URL, so it doesn't get inline autocompleted
    // as aggressively later.
    match.transition = content::PAGE_TRANSITION_LINK;
  }

  const TemplateURL* template_url = match.GetTemplateURL(profile_, false);
  if (template_url && template_url->url_ref().HasGoogleBaseURLs())
    GoogleURLTracker::GoogleURLSearchCommitted(profile_);

  view_->OpenMatch(match, disposition, alternate_nav_url,
                   OmniboxPopupModel::kNoMatch);
}

void OmniboxEditModel::OpenMatch(const AutocompleteMatch& match,
                                 WindowOpenDisposition disposition,
                                 const GURL& alternate_nav_url,
                                 size_t index) {
  // We only care about cases where there is a selection (i.e. the popup is
  // open).
  if (popup_model()->IsOpen()) {
    const base::TimeTicks& now(base::TimeTicks::Now());
    base::TimeDelta elapsed_time_since_user_first_modified_omnibox(
        now - time_user_first_modified_omnibox_);
    base::TimeDelta elapsed_time_since_last_change_to_default_match(
        now - autocomplete_controller()->last_time_default_match_changed());
    // These elapsed times don't really make sense for ZeroSuggest matches
    // (because the user does not modify the omnibox for ZeroSuggest), so for
    // those we set the elapsed times to something that will be ignored by
    // metrics_log.cc.
    if (match.provider &&
        match.provider->type() == AutocompleteProvider::TYPE_ZERO_SUGGEST) {
      elapsed_time_since_user_first_modified_omnibox =
          base::TimeDelta::FromMilliseconds(-1);
      elapsed_time_since_last_change_to_default_match =
          base::TimeDelta::FromMilliseconds(-1);
    }
    // TODO(sreeram): Handle is_temporary_text_set_by_instant_ correctly.
    OmniboxLog log(
        autocomplete_controller()->input().text(),
        just_deleted_text_,
        autocomplete_controller()->input().type(),
        popup_model()->selected_line(),
        -1,  // don't yet know tab ID; set later if appropriate
        delegate_->CurrentPageExists() ? ClassifyPage(delegate_->GetURL()) :
            metrics::OmniboxEventProto_PageClassification_OTHER,
        elapsed_time_since_user_first_modified_omnibox,
        string16::npos,  // completed_length; possibly set later
        elapsed_time_since_last_change_to_default_match,
        result());

    DCHECK(user_input_in_progress_ || (match.provider &&
           match.provider->type() == AutocompleteProvider::TYPE_ZERO_SUGGEST))
        << "We didn't get here through the expected series of calls. "
        << "time_user_first_modified_omnibox_ is not set correctly and other "
        << "things may be wrong. Match provider: "
        << (match.provider ? match.provider->GetName() : "NULL");
    DCHECK(log.elapsed_time_since_user_first_modified_omnibox >=
           log.elapsed_time_since_last_change_to_default_match)
        << "We should've got the notification that the user modified the "
        << "omnibox text at same time or before the most recent time the "
        << "default match changed.";

    if (index != OmniboxPopupModel::kNoMatch)
      log.selected_index = index;
    if (match.inline_autocomplete_offset != string16::npos) {
      DCHECK_GE(match.fill_into_edit.length(),
                match.inline_autocomplete_offset);
      log.completed_length =
          match.fill_into_edit.length() - match.inline_autocomplete_offset;
    }

    if ((disposition == CURRENT_TAB) && delegate_->CurrentPageExists()) {
      // If we know the destination is being opened in the current tab,
      // we can easily get the tab ID.  (If it's being opened in a new
      // tab, we don't know the tab ID yet.)
      log.tab_id = delegate_->GetSessionID().id();
    }
    autocomplete_controller()->AddProvidersInfo(&log.providers_info);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
        content::Source<Profile>(profile_),
        content::Details<OmniboxLog>(&log));
    HISTOGRAM_ENUMERATION("Omnibox.EventCount", 1, 2);
  }

  TemplateURL* template_url = match.GetTemplateURL(profile_, false);
  if (template_url) {
    if (match.transition == content::PAGE_TRANSITION_KEYWORD) {
      // The user is using a non-substituting keyword or is explicitly in
      // keyword mode.
      const AutocompleteMatch& match = (index == OmniboxPopupModel::kNoMatch) ?
          CurrentMatch(NULL) : result().match_at(index);

      // Don't increment usage count for extension keywords.
      if (delegate_->ProcessExtensionKeyword(template_url, match,
                                             disposition)) {
        if (disposition != NEW_BACKGROUND_TAB)
          view_->RevertAll();
        return;
      }

      content::RecordAction(UserMetricsAction("AcceptedKeyword"));
      TemplateURLServiceFactory::GetForProfile(profile_)->IncrementUsageCount(
          template_url);
    } else {
      DCHECK_EQ(content::PAGE_TRANSITION_GENERATED, match.transition);
      // NOTE: We purposefully don't increment the usage count of the default
      // search engine here like we do for explicit keywords above; see comments
      // in template_url.h.
    }

    // NOTE: Non-prepopulated engines will all have ID 0, which is fine as
    // the prepopulate IDs start at 1.  Distribution-specific engines will
    // all have IDs above the maximum, and will be automatically lumped
    // together in an "overflow" bucket in the histogram.
    UMA_HISTOGRAM_ENUMERATION("Omnibox.SearchEngine",
        template_url->prepopulate_id(),
        TemplateURLPrepopulateData::kMaxPrepopulatedEngineID);
  }

  if (disposition != NEW_BACKGROUND_TAB) {
    base::AutoReset<bool> tmp(&in_revert_, true);
    view_->RevertAll();  // Revert the box to its unedited state
  }

  if (match.type == AutocompleteMatchType::EXTENSION_APP) {
    ExtensionAppProvider::LaunchAppFromOmnibox(match, profile_, disposition);
  } else {
    base::TimeDelta query_formulation_time =
        base::TimeTicks::Now() - time_user_first_modified_omnibox_;
    const GURL destination_url = autocomplete_controller()->
        GetDestinationURL(match, query_formulation_time);

    // Track whether the destination URL sends us to a search results page
    // using the default search provider.
    TemplateURL* default_provider =
        TemplateURLServiceFactory::GetForProfile(profile_)->
            GetDefaultSearchProvider();
    if (default_provider && default_provider->IsSearchURL(destination_url))
      content::RecordAction(UserMetricsAction(
          "OmniboxDestinationURLMatchesDefaultSearchProvider"));

#if defined(HTML_INSTANT_EXTENDED_POPUP)
    // If running with instant, notify the instant controller that a navigation
    // is about to take place if we are navigating to a URL. This can be
    // determined by inspecting the transition type. To ensure that this is only
    // done on Enter key press, check that the disposition is CURRENT_TAB. This
    // is the same heuristic used by BrowserInstantController::OpenInstant
    if (match.transition == content::PAGE_TRANSITION_TYPED &&
        disposition == CURRENT_TAB) {
      InstantController* instant = GetInstantController();
      if (instant)
        instant->OmniboxNavigateToURL();
    }
#endif

    // This calls RevertAll again.
    base::AutoReset<bool> tmp(&in_revert_, true);
    controller_->OnAutocompleteAccept(destination_url, disposition,
                                      match.transition, alternate_nav_url);
  }

  if (match.starred)
    bookmark_utils::RecordBookmarkLaunch(bookmark_utils::LAUNCH_OMNIBOX);
}

bool OmniboxEditModel::AcceptKeyword(EnteredKeywordModeMethod entered_method) {
  DCHECK(is_keyword_hint_ && !keyword_.empty());

  autocomplete_controller()->Stop(false);
  is_keyword_hint_ = false;

  if (popup_model()->IsOpen())
    popup_model()->SetSelectedLineState(OmniboxPopupModel::KEYWORD);
  else
    StartAutocomplete(false, true);

  // Ensure the current selection is saved before showing keyword mode
  // so that moving to another line and then reverting the text will restore
  // the current state properly.
  bool save_original_selection = !has_temporary_text_;
  has_temporary_text_ = true;
  is_temporary_text_set_by_instant_ = false;
  selected_instant_autocomplete_match_index_ = OmniboxPopupModel::kNoMatch;
  is_instant_temporary_text_a_search_query_ = false;
  view_->OnTemporaryTextMaybeChanged(
      DisplayTextFromUserText(CurrentMatch(NULL).fill_into_edit),
      save_original_selection, true);

  content::RecordAction(UserMetricsAction("AcceptedKeywordHint"));
  UMA_HISTOGRAM_ENUMERATION(kEnteredKeywordModeHistogram, entered_method,
                            ENTERED_KEYWORD_MODE_NUM_ITEMS);

  return true;
}

void OmniboxEditModel::AcceptTemporaryTextAsUserText() {
  InternalSetUserText(UserTextFromDisplayText(view_->GetText()));
  has_temporary_text_ = false;
  is_temporary_text_set_by_instant_ = false;
  selected_instant_autocomplete_match_index_ = OmniboxPopupModel::kNoMatch;
  is_instant_temporary_text_a_search_query_ = false;
  OnPopupBoundsChanged(gfx::Rect());
  delegate_->NotifySearchTabHelper(user_input_in_progress_, !in_revert_,
                                   popup_model()->IsOpen(), user_text_.empty());
}

void OmniboxEditModel::ClearKeyword(const string16& visible_text) {
  autocomplete_controller()->Stop(false);
  omnibox_controller_->ClearPopupKeywordMode();

  const string16 window_text(keyword_ + visible_text);

  // Only reset the result if the edit text has changed since the
  // keyword was accepted, or if the popup is closed.
  if (just_deleted_text_ || !visible_text.empty() || !popup_model()->IsOpen()) {
    view_->OnBeforePossibleChange();
    view_->SetWindowTextAndCaretPos(window_text.c_str(), keyword_.length(),
        false, false);
    keyword_.clear();
    is_keyword_hint_ = false;
    view_->OnAfterPossibleChange();
    just_deleted_text_ = true;  // OnAfterPossibleChange() fails to clear this
                                // since the edit contents have actually grown
                                // longer.
  } else {
    is_keyword_hint_ = true;
    view_->SetWindowTextAndCaretPos(window_text.c_str(), keyword_.length(),
        false, true);
  }
}

void OmniboxEditModel::OnSetFocus(bool control_down) {
  // If the omnibox lost focus while the caret was hidden and then regained
  // focus, OnSetFocus() is called and should restore visibility. Note that
  // focus can be regained without an accompanying call to
  // OmniboxView::SetFocus(), e.g. by tabbing in.
  SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  control_key_state_ = control_down ? DOWN_WITHOUT_CHANGE : UP;

  // Try to get ZeroSuggest suggestions if a page is loaded and the user has
  // not been typing in the omnibox.  The |user_input_in_progress_| check is
  // used to detect the case where this function is called after right-clicking
  // in the omnibox and selecting paste in Linux (in which case we actually get
  // the OnSetFocus() call after the process of handling the paste has kicked
  // off).
  // TODO(hfung): Remove this when crbug/271590 is fixed.
  if (delegate_->CurrentPageExists() && !user_input_in_progress_) {
    // TODO(jered): We may want to merge this into Start() and just call that
    // here rather than having a special entry point for zero-suggest.  Note
    // that we avoid PermanentURL() here because it's not guaranteed to give us
    // the actual underlying current URL, e.g. if we're on the NTP and the
    // |permanent_text_| is empty.
    autocomplete_controller()->StartZeroSuggest(delegate_->GetURL(),
                                                permanent_text_);
  }

  delegate_->NotifySearchTabHelper(user_input_in_progress_, !in_revert_,
                                   popup_model()->IsOpen(), user_text_.empty());
}

void OmniboxEditModel::SetCaretVisibility(bool visible) {
  // Caret visibility only matters if the omnibox has focus.
  if (focus_state_ != OMNIBOX_FOCUS_NONE) {
    SetFocusState(visible ? OMNIBOX_FOCUS_VISIBLE : OMNIBOX_FOCUS_INVISIBLE,
                  OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  }
}

void OmniboxEditModel::OnWillKillFocus(gfx::NativeView view_gaining_focus) {
  InstantController* instant = GetInstantController();
  if (instant) {
    instant->OmniboxFocusChanged(OMNIBOX_FOCUS_NONE,
                                 OMNIBOX_FOCUS_CHANGE_EXPLICIT,
                                 view_gaining_focus);
  }

  // TODO(jered): Rip this out along with StartZeroSuggest.
  autocomplete_controller()->StopZeroSuggest();
  delegate_->NotifySearchTabHelper(user_input_in_progress_, !in_revert_,
                                   popup_model()->IsOpen(), user_text_.empty());
}

void OmniboxEditModel::OnKillFocus() {
  // TODO(samarth): determine if it is safe to move the call to
  // OmniboxFocusChanged() from OnWillKillFocus() to here, which would let us
  // just call SetFocusState() to handle the state change.
  focus_state_ = OMNIBOX_FOCUS_NONE;
  control_key_state_ = UP;
  paste_state_ = NONE;
}

bool OmniboxEditModel::OnEscapeKeyPressed() {
  if (has_temporary_text_) {
    if (CurrentMatch(NULL).destination_url != original_url_) {
      RevertTemporaryText(true);
      return true;
    }
  }

  // We do not clear the pending entry from the omnibox when a load is first
  // stopped.  If the user presses Escape while stopped, we clear it.
  if (delegate_->CurrentPageExists() && !delegate_->IsLoading()) {
    delegate_->GetNavigationController().DiscardNonCommittedEntries();
    view_->Update(NULL);
  }

  // If the user wasn't editing, but merely had focus in the edit, allow <esc>
  // to be processed as an accelerator, so it can still be used to stop a load.
  // When the permanent text isn't all selected we still fall through to the
  // SelectAll() call below so users can arrow around in the text and then hit
  // <esc> to quickly replace all the text; this matches IE.
  if (!user_input_in_progress_ && view_->IsSelectAll())
    return false;

  in_escape_handler_ = true;
  if (!user_text_.empty()) {
    UMA_HISTOGRAM_ENUMERATION(kOmniboxUserTextClearedHistogram,
                              OMNIBOX_USER_TEXT_CLEARED_WITH_ESCAPE,
                              OMNIBOX_USER_TEXT_CLEARED_NUM_OF_ITEMS);
  }
  view_->RevertAll();
  in_escape_handler_ = false;
  view_->SelectAll(true);
  return true;
}

void OmniboxEditModel::OnControlKeyChanged(bool pressed) {
  if (pressed == (control_key_state_ == UP))
    control_key_state_ = pressed ? DOWN_WITHOUT_CHANGE : UP;
}

void OmniboxEditModel::OnUpOrDownKeyPressed(int count) {
  // NOTE: This purposefully doesn't trigger any code that resets paste_state_.
  if (!popup_model()->IsOpen()) {
    if (!query_in_progress()) {
      // The popup is neither open nor working on a query already.  So, start an
      // autocomplete query for the current text.  This also sets
      // user_input_in_progress_ to true, which we want: if the user has started
      // to interact with the popup, changing the permanent_text_ shouldn't
      // change the displayed text.
      // Note: This does not force the popup to open immediately.
      // TODO(pkasting): We should, in fact, force this particular query to open
      // the popup immediately.
      if (!user_input_in_progress_)
        InternalSetUserText(permanent_text_);
      view_->UpdatePopup();
    } else {
      // TODO(pkasting): The popup is working on a query but is not open.  We
      // should force it to open immediately.
    }
  } else {
#if defined(HTML_INSTANT_EXTENDED_POPUP)
    InstantController* instant = GetInstantController();
    if (instant && instant->OnUpOrDownKeyPressed(count)) {
      // If Instant handles the key press, it's showing a list of suggestions
      // that it's stepping through. In that case, our popup model is
      // irrelevant, so don't process the key press ourselves. However, do stop
      // the autocomplete system from changing the results.
      autocomplete_controller()->Stop(false);
    } else
#endif
    {
      // The popup is open, so the user should be able to interact with it
      // normally.
      popup_model()->Move(count);
    }
  }
}

void OmniboxEditModel::OnPopupDataChanged(
    const string16& text,
    GURL* destination_for_temporary_text_change,
    const string16& keyword,
    bool is_keyword_hint) {
  // The popup changed its data, the match in the controller is no longer valid.
  omnibox_controller_->InvalidateCurrentMatch();

  // Update keyword/hint-related local state.
  bool keyword_state_changed = (keyword_ != keyword) ||
      ((is_keyword_hint_ != is_keyword_hint) && !keyword.empty());
  if (keyword_state_changed) {
    keyword_ = keyword;
    is_keyword_hint_ = is_keyword_hint;

    // |is_keyword_hint_| should always be false if |keyword_| is empty.
    DCHECK(!keyword_.empty() || !is_keyword_hint_);
  }

  // Handle changes to temporary text.
  if (destination_for_temporary_text_change != NULL) {
    const bool save_original_selection = !has_temporary_text_;
    if (save_original_selection) {
      // Save the original selection and URL so it can be reverted later.
      has_temporary_text_ = true;
      is_temporary_text_set_by_instant_ = false;
      selected_instant_autocomplete_match_index_ = OmniboxPopupModel::kNoMatch;
      is_instant_temporary_text_a_search_query_ = false;
      original_url_ = *destination_for_temporary_text_change;
      inline_autocomplete_text_.clear();
    }
    if (control_key_state_ == DOWN_WITHOUT_CHANGE) {
      // Arrowing around the popup cancels control-enter.
      control_key_state_ = DOWN_WITH_CHANGE;
      // Now things are a bit screwy: the desired_tld has changed, but if we
      // update the popup, the new order of entries won't match the old, so the
      // user's selection gets screwy; and if we don't update the popup, and the
      // user reverts, then the selected item will be as if control is still
      // pressed, even though maybe it isn't any more.  There is no obvious
      // right answer here :(
    }
    view_->OnTemporaryTextMaybeChanged(DisplayTextFromUserText(text),
                                       save_original_selection, true);
    return;
  }

  bool call_controller_onchanged = true;
  inline_autocomplete_text_ = text;

  string16 user_text = user_input_in_progress_ ? user_text_ : permanent_text_;
  if (keyword_state_changed && KeywordIsSelected()) {
    // If we reach here, the user most likely entered keyword mode by inserting
    // a space between a keyword name and a search string (as pressing space or
    // tab after the keyword name alone would have been be handled in
    // MaybeAcceptKeywordBySpace() by calling AcceptKeyword(), which won't reach
    // here).  In this case, we don't want to call
    // OnInlineAutocompleteTextMaybeChanged() as normal, because that will
    // correctly change the text (to the search string alone) but move the caret
    // to the end of the string; instead we want the caret at the start of the
    // search string since that's where it was in the original input.  So we set
    // the text and caret position directly.
    //
    // It may also be possible to reach here if we're reverting from having
    // temporary text back to a default match that's a keyword search, but in
    // that case the RevertTemporaryText() call below will reset the caret or
    // selection correctly so the caret positioning we do here won't matter.
    view_->SetWindowTextAndCaretPos(DisplayTextFromUserText(user_text), 0,
                                                            false, false);
  } else if (view_->OnInlineAutocompleteTextMaybeChanged(
             DisplayTextFromUserText(user_text + inline_autocomplete_text_),
             DisplayTextFromUserText(user_text).length())) {
    call_controller_onchanged = false;
  }

  // If |has_temporary_text_| is true, then we previously had a manual selection
  // but now don't (or |destination_for_temporary_text_change| would have been
  // non-NULL). This can happen when deleting the selected item in the popup.
  // In this case, we've already reverted the popup to the default match, so we
  // need to revert ourselves as well.
  if (has_temporary_text_) {
    RevertTemporaryText(false);
    call_controller_onchanged = false;
  }

  // We need to invoke OnChanged in case the destination url changed (as could
  // happen when control is toggled).
  if (call_controller_onchanged)
    OnChanged();
}

bool OmniboxEditModel::OnAfterPossibleChange(const string16& old_text,
                                             const string16& new_text,
                                             size_t selection_start,
                                             size_t selection_end,
                                             bool selection_differs,
                                             bool text_differs,
                                             bool just_deleted_text,
                                             bool allow_keyword_ui_change) {
  // Update the paste state as appropriate: if we're just finishing a paste
  // that replaced all the text, preserve that information; otherwise, if we've
  // made some other edit, clear paste tracking.
  if (paste_state_ == PASTING)
    paste_state_ = PASTED;
  else if (text_differs)
    paste_state_ = NONE;

  // Restore caret visibility whenever the user changes text or selection in the
  // omnibox.
  if (text_differs || selection_differs)
    SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_TYPING);

  // Modifying the selection counts as accepting the autocompleted text.
  const bool user_text_changed =
      text_differs || (selection_differs && !inline_autocomplete_text_.empty());

  // If something has changed while the control key is down, prevent
  // "ctrl-enter" until the control key is released.
  if ((text_differs || selection_differs) &&
      (control_key_state_ == DOWN_WITHOUT_CHANGE))
    control_key_state_ = DOWN_WITH_CHANGE;

  if (!user_text_changed)
    return false;

  // If the user text has not changed, we do not want to change the model's
  // state associated with the text.  Otherwise, we can get surprising behavior
  // where the autocompleted text unexpectedly reappears, e.g. crbug.com/55983
  InternalSetUserText(UserTextFromDisplayText(new_text));
  has_temporary_text_ = false;
    is_temporary_text_set_by_instant_ = false;
    selected_instant_autocomplete_match_index_ = OmniboxPopupModel::kNoMatch;
    is_instant_temporary_text_a_search_query_ = false;

  // Track when the user has deleted text so we won't allow inline
  // autocomplete.
  just_deleted_text_ = just_deleted_text;

  if (user_input_in_progress_ && user_text_.empty()) {
    // Log cases where the user started editing and then subsequently cleared
    // all the text.  Note that this explicitly doesn't catch cases like
    // "hit ctrl-l to select whole edit contents, then hit backspace", because
    // in such cases, |user_input_in_progress| won't be true here.
    UMA_HISTOGRAM_ENUMERATION(kOmniboxUserTextClearedHistogram,
                              OMNIBOX_USER_TEXT_CLEARED_BY_EDITING,
                              OMNIBOX_USER_TEXT_CLEARED_NUM_OF_ITEMS);
  }

  const bool no_selection = selection_start == selection_end;

  // Update the popup for the change, in the process changing to keyword mode
  // if the user hit space in mid-string after a keyword.
  // |allow_exact_keyword_match_| will be used by StartAutocomplete() method,
  // which will be called by |view_->UpdatePopup()|; so after that returns we
  // can safely reset this flag.
  allow_exact_keyword_match_ = text_differs && allow_keyword_ui_change &&
      !just_deleted_text && no_selection &&
      CreatedKeywordSearchByInsertingSpaceInMiddle(old_text, user_text_,
                                                   selection_start);
  if (allow_exact_keyword_match_) {
    UMA_HISTOGRAM_ENUMERATION(kEnteredKeywordModeHistogram,
                              ENTERED_KEYWORD_MODE_VIA_SPACE_IN_MIDDLE,
                              ENTERED_KEYWORD_MODE_NUM_ITEMS);
  }
  view_->UpdatePopup();
  allow_exact_keyword_match_ = false;

  // Change to keyword mode if the user is now pressing space after a keyword
  // name.  Note that if this is the case, then even if there was no keyword
  // hint when we entered this function (e.g. if the user has used space to
  // replace some selected text that was adjoined to this keyword), there will
  // be one now because of the call to UpdatePopup() above; so it's safe for
  // MaybeAcceptKeywordBySpace() to look at |keyword_| and |is_keyword_hint_| to
  // determine what keyword, if any, is applicable.
  //
  // If MaybeAcceptKeywordBySpace() accepts the keyword and returns true, that
  // will have updated our state already, so in that case we don't also return
  // true from this function.
  return !(text_differs && allow_keyword_ui_change && !just_deleted_text &&
           no_selection && (selection_start == user_text_.length()) &&
           MaybeAcceptKeywordBySpace(user_text_));
}

void OmniboxEditModel::OnCurrentMatchChanged(bool is_temporary_set_by_instant) {
  has_temporary_text_ = is_temporary_set_by_instant;
  is_temporary_text_set_by_instant_ = is_temporary_set_by_instant;

  const AutocompleteMatch& match = omnibox_controller_->current_match();

  if (is_temporary_set_by_instant) {
    view_->OnTemporaryTextMaybeChanged(
        DisplayTextFromUserText(match.fill_into_edit), !has_temporary_text_,
        false);
  } else {
    // We store |keyword| and |is_keyword_hint| in temporary variables since
    // OnPopupDataChanged use their previous state to detect changes.
    string16 keyword;
    bool is_keyword_hint;
    match.GetKeywordUIState(profile_, &keyword, &is_keyword_hint);
    string16 inline_autocomplete_text;
    if (match.inline_autocomplete_offset < match.fill_into_edit.length()) {
      // We have blue text, go through OnPopupDataChanged.
      // TODO(beaudoin): Merge OnPopupDataChanged with this method once the
      // popup handling has completely migrated to omnibox_controller.
      inline_autocomplete_text =
          match.fill_into_edit.substr(match.inline_autocomplete_offset);
    }
    popup_model()->OnResultChanged();
    OnPopupDataChanged(inline_autocomplete_text, NULL, keyword,
                       is_keyword_hint);
  }
}

void OmniboxEditModel::OnGrayTextChanged() {
  view_->SetInstantSuggestion(omnibox_controller_->gray_suggestion());
}

string16 OmniboxEditModel::GetViewText() const {
  return view_->GetText();
}

InstantController* OmniboxEditModel::GetInstantController() const {
  return controller_->GetInstant();
}

bool OmniboxEditModel::query_in_progress() const {
  return !autocomplete_controller()->done();
}

void OmniboxEditModel::InternalSetUserText(const string16& text) {
  user_text_ = text;
  just_deleted_text_ = false;
  inline_autocomplete_text_.clear();
}

bool OmniboxEditModel::KeywordIsSelected() const {
  return !is_keyword_hint_ && !keyword_.empty();
}

void OmniboxEditModel::ClearPopupKeywordMode() const {
  omnibox_controller_->ClearPopupKeywordMode();
}

string16 OmniboxEditModel::DisplayTextFromUserText(const string16& text) const {
  return KeywordIsSelected() ?
      KeywordProvider::SplitReplacementStringFromInput(text, false) : text;
}

string16 OmniboxEditModel::UserTextFromDisplayText(const string16& text) const {
  return KeywordIsSelected() ? (keyword_ + char16(' ') + text) : text;
}

void OmniboxEditModel::GetInfoForCurrentText(AutocompleteMatch* match,
                                             GURL* alternate_nav_url) const {
  DCHECK(match != NULL);

  if (!user_input_in_progress_ &&
      view_->toolbar_model()->WouldReplaceSearchURLWithSearchTerms()) {
    // Any time the user hits enter on the unchanged omnibox, we should reload.
    // When we're not extracting search terms, AcceptInput() will take care of
    // this (see code referring to PAGE_TRANSITION_RELOAD there), but when we're
    // extracting search terms, the conditionals there won't fire, so we
    // explicitly set up a match that will reload here.

    // It's important that we fetch the current visible URL to reload instead of
    // just getting a "search what you typed" URL from
    // SearchProvider::CreateSearchSuggestion(), since the user may be in a
    // non-default search mode such as image search.
    match->type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
    match->destination_url =
        delegate_->GetNavigationController().GetVisibleEntry()->GetURL();
    match->transition = content::PAGE_TRANSITION_RELOAD;
#if defined(HTML_INSTANT_EXTENDED_POPUP)
  } else if (is_temporary_text_set_by_instant_) {
    // If there's temporary text and it has been set by Instant, we won't find
    // it in the popup model, so create the match based on the type Instant told
    // us (SWYT for queries and UWYT for URLs). We do this instead of
    // classifying the text ourselves because the text may look like a URL, but
    // Instant may expect it to be a search (e.g.: a query for "amazon.com").
    if (selected_instant_autocomplete_match_index_ !=
            OmniboxPopupModel::kNoMatch) {
      // Great, we know the exact match struct. Just use that.
      const AutocompleteResult& result = this->result();
      *match = result.match_at(selected_instant_autocomplete_match_index_);
    } else {
      const string16& text = view_->GetText();
      AutocompleteInput input(text, string16::npos, string16(), GURL(), false,
                              false, false, AutocompleteInput::BEST_MATCH);
      // Only the destination_url and the transition of the match will be be
      // used (to either navigate to the URL or let Instant commit its preview).
      // The match won't be used for logging, displaying in the dropdown, etc.
      // So, it's okay to pass in mostly bogus params (such as relevance = 0).
      // TODO(sreeram): Always using NO_SUGGESTIONS_AVAILABLE is wrong when
      // Instant is using the local fallback overlay. Fix.
      if (is_instant_temporary_text_a_search_query_) {
        const TemplateURL* default_provider =
            TemplateURLServiceFactory::GetForProfile(profile_)->
                GetDefaultSearchProvider();
        if (default_provider && default_provider->SupportsReplacement()) {
          *match = SearchProvider::CreateSearchSuggestion(profile_,
              autocomplete_controller()->search_provider(), input, text, text,
              0, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
              TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, false,
              default_provider->keyword(),
              controller_->GetOmniboxBounds().x());
        } else {
          // Can't create a new search match. Leave |match| as is, with an
          // invalid destination_url. This shouldn't ever happen. For example,
          // even if a group policy update in the midst of interacting with
          // Instant leaves us without a valid search provider, Instant
          // should've observed the update and reset
          // |is_temporary_text_set_by_instant_|, so we still shouldn't get
          // here. However, as protection against the unknowns and Instant
          // regressions, we simply return an invalid match instead of crashing
          // (hence no DCHECK).
        }
      } else {
        *match = HistoryURLProvider::SuggestExactInput(
            autocomplete_controller()->history_url_provider(), input, false);
      }
    }
#endif
  } else if (popup_model()->IsOpen() || query_in_progress()) {
    if (query_in_progress()) {
      // It's technically possible for |result| to be empty if no provider
      // returns a synchronous result but the query has not completed
      // synchronously; pratically, however, that should never actually happen.
      if (result().empty())
        return;
      // The user cannot have manually selected a match, or the query would have
      // stopped. So the default match must be the desired selection.
      *match = *result().default_match();
    } else {
      // If there are no results, the popup should be closed, so we shouldn't
      // have gotten here.
      CHECK(!result().empty());
      CHECK(popup_model()->selected_line() < result().size());
      *match = result().match_at(popup_model()->selected_line());
    }
    if (alternate_nav_url && popup_model()->manually_selected_match().empty())
      *alternate_nav_url = result().alternate_nav_url();
  } else {
    AutocompleteClassifierFactory::GetForProfile(profile_)->Classify(
        UserTextFromDisplayText(view_->GetText()), KeywordIsSelected(), true,
        match, alternate_nav_url);
  }
}

void OmniboxEditModel::RevertTemporaryText(bool revert_popup) {
  // The user typed something, then selected a different item.  Restore the
  // text they typed and change back to the default item.
  // NOTE: This purposefully does not reset paste_state_.
#if defined(HTML_INSTANT_EXTENDED_POPUP)
  bool notify_instant = is_temporary_text_set_by_instant_;
#endif
  just_deleted_text_ = false;
  has_temporary_text_ = false;
  is_temporary_text_set_by_instant_ = false;
  selected_instant_autocomplete_match_index_ = OmniboxPopupModel::kNoMatch;
  is_instant_temporary_text_a_search_query_ = false;

#if defined(HTML_INSTANT_EXTENDED_POPUP)
  InstantController* instant = GetInstantController();
  if (instant && notify_instant) {
    // Normally, popup_model()->ResetToDefaultMatch() will cause the view text
    // to be updated. In Instant Extended mode however, the popup_model() is
    // not used, so it won't do anything. So, update the view ourselves. Even
    // if Instant is not in extended mode (i.e., it's enabled in non-extended
    // mode, or disabled altogether), this is okay to do, since the call to
    // popup_model()->ResetToDefaultMatch() will just override whatever we do
    // here.
    //
    // The two "false" arguments make sure that our shenanigans don't cause any
    // previously saved selection to be erased nor OnChanged() to be called.
    view_->OnTemporaryTextMaybeChanged(user_text_ + inline_autocomplete_text_,
        false, false);
    AutocompleteResult::const_iterator match(result().default_match());
    instant->OnCancel(match != result().end() ? *match : AutocompleteMatch(),
                      user_text_,
                      user_text_ + inline_autocomplete_text_);
  }
#endif
  if (revert_popup)
    popup_model()->ResetToDefaultMatch();
  view_->OnRevertTemporaryText();
}

bool OmniboxEditModel::MaybeAcceptKeywordBySpace(const string16& new_text) {
  size_t keyword_length = new_text.length() - 1;
  return (paste_state_ == NONE) && is_keyword_hint_ && !keyword_.empty() &&
      inline_autocomplete_text_.empty() &&
      (keyword_.length() == keyword_length) &&
      IsSpaceCharForAcceptingKeyword(new_text[keyword_length]) &&
      !new_text.compare(0, keyword_length, keyword_, 0, keyword_length) &&
      AcceptKeyword(ENTERED_KEYWORD_MODE_VIA_SPACE_AT_END);
}

bool OmniboxEditModel::CreatedKeywordSearchByInsertingSpaceInMiddle(
    const string16& old_text,
    const string16& new_text,
    size_t caret_position) const {
  DCHECK_GE(new_text.length(), caret_position);

  // Check simple conditions first.
  if ((paste_state_ != NONE) || (caret_position < 2) ||
      (old_text.length() < caret_position) ||
      (new_text.length() == caret_position))
    return false;
  size_t space_position = caret_position - 1;
  if (!IsSpaceCharForAcceptingKeyword(new_text[space_position]) ||
      IsWhitespace(new_text[space_position - 1]) ||
      new_text.compare(0, space_position, old_text, 0, space_position) ||
      !new_text.compare(space_position, new_text.length() - space_position,
                        old_text, space_position,
                        old_text.length() - space_position)) {
    return false;
  }

  // Then check if the text before the inserted space matches a keyword.
  string16 keyword;
  TrimWhitespace(new_text.substr(0, space_position), TRIM_LEADING, &keyword);
  // TODO(sreeram): Once the Instant extended API supports keywords properly,
  // keyword_provider() should never be NULL. Remove that clause.
  return !keyword.empty() && autocomplete_controller()->keyword_provider() &&
      !autocomplete_controller()->keyword_provider()->
          GetKeywordForText(keyword).empty();
}

//  static
bool OmniboxEditModel::IsSpaceCharForAcceptingKeyword(wchar_t c) {
  switch (c) {
    case 0x0020:  // Space
    case 0x3000:  // Ideographic Space
      return true;
    default:
      return false;
  }
}

metrics::OmniboxEventProto::PageClassification
    OmniboxEditModel::ClassifyPage(const GURL& gurl) const {
  if (!gurl.is_valid())
    return metrics::OmniboxEventProto_PageClassification_INVALID_SPEC;
  const std::string& url = gurl.spec();
  if (url == chrome::kChromeUINewTabURL)
    return metrics::OmniboxEventProto_PageClassification_NEW_TAB_PAGE;
  if (url == content::kAboutBlankURL)
    return metrics::OmniboxEventProto_PageClassification_BLANK;
  if (url == profile()->GetPrefs()->GetString(prefs::kHomePage))
    return metrics::OmniboxEventProto_PageClassification_HOMEPAGE;
  return metrics::OmniboxEventProto_PageClassification_OTHER;
}

void OmniboxEditModel::ClassifyStringForPasteAndGo(
    const string16& text,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) const {
  DCHECK(match);
  AutocompleteClassifierFactory::GetForProfile(profile_)->Classify(text,
      false, false, match, alternate_nav_url);
}

void OmniboxEditModel::SetFocusState(OmniboxFocusState state,
                                     OmniboxFocusChangeReason reason) {
  if (state == focus_state_)
    return;

  InstantController* instant = GetInstantController();
  if (instant)
    instant->OmniboxFocusChanged(state, reason, NULL);

  // Update state and notify view if the omnibox has focus and the caret
  // visibility changed.
  const bool was_caret_visible = is_caret_visible();
  focus_state_ = state;
  if (focus_state_ != OMNIBOX_FOCUS_NONE &&
      is_caret_visible() != was_caret_visible)
    view_->ApplyCaretVisibility();
}
