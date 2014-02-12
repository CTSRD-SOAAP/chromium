// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/automatic_profile_resetter_delegate.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/profile_reset_global_error.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "chrome/browser/enumerate_modules_model_win.h"
#endif

namespace {

const char kTestBrandcode[] = "FOOBAR";

const char kTestHomepage[] = "http://google.com";
const char kTestBrandedHomepage[] = "http://example.com";

const ProfileResetter::ResettableFlags kResettableAspectsForTest =
    ProfileResetter::ALL & ~ProfileResetter::COOKIES_AND_SITE_DATA;

// Helpers -------------------------------------------------------------------

// A testing version of the AutomaticProfileResetterDelegate that differs from
// the real one only in that it has its feedback reporting mocked out, and it
// will not reset COOKIES_AND_SITE_DATA, due to difficulties to set up some
// required URLRequestContexts in unit tests.
class AutomaticProfileResetterDelegateUnderTest
    : public AutomaticProfileResetterDelegateImpl {
 public:
  explicit AutomaticProfileResetterDelegateUnderTest(Profile* profile)
      : AutomaticProfileResetterDelegateImpl(
            profile, kResettableAspectsForTest) {}
  virtual ~AutomaticProfileResetterDelegateUnderTest() {}

  MOCK_CONST_METHOD1(SendFeedback, void(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterDelegateUnderTest);
};

class MockCallbackTarget {
 public:
  MockCallbackTarget() {}
  ~MockCallbackTarget() {}

  MOCK_CONST_METHOD0(Run, void(void));

  base::Closure CreateClosure() {
    return base::Bind(&MockCallbackTarget::Run, base::Unretained(this));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallbackTarget);
};

// Returns the details of the default search provider from |prefs| in a format
// suitable for usage as |expected_details| in ExpectDetailsMatch().
scoped_ptr<base::DictionaryValue> GetDefaultSearchProviderDetailsFromPrefs(
    const PrefService* prefs) {
  const char kDefaultSearchProviderPrefix[] = "default_search_provider";
  scoped_ptr<base::DictionaryValue> pref_values_with_path_expansion(
      prefs->GetPreferenceValues());
  const base::DictionaryValue* dsp_details = NULL;
  EXPECT_TRUE(pref_values_with_path_expansion->GetDictionary(
      kDefaultSearchProviderPrefix, &dsp_details));
  return scoped_ptr<base::DictionaryValue>(
      dsp_details ? dsp_details->DeepCopy() : new base::DictionaryValue);
}

// Verifies that the |details| of a search engine as provided by the delegate
// are correct in comparison to the |expected_details| coming from the Prefs.
void ExpectDetailsMatch(const base::DictionaryValue& expected_details,
                        const base::DictionaryValue& details) {
  for (base::DictionaryValue::Iterator it(expected_details); !it.IsAtEnd();
       it.Advance()) {
    SCOPED_TRACE(testing::Message("Key: ") << it.key());
    if (it.key() == "enabled" || it.key() == "synced_guid") {
      // These attributes should not be present.
      EXPECT_FALSE(details.HasKey(it.key()));
      continue;
    }
    const base::Value* expected_value = &it.value();
    const base::Value* actual_value = NULL;
    ASSERT_TRUE(details.Get(it.key(), &actual_value));
    if (it.key() == "id") {
      // Ignore ID as it is dynamically assigned by the TemplateURLService.
    } else if (it.key() == "encodings") {
      // Encoding list is stored in Prefs as a single string with tokens
      // delimited by semicolons.
      std::string expected_encodings;
      ASSERT_TRUE(expected_value->GetAsString(&expected_encodings));
      const base::ListValue* actual_encodings_list = NULL;
      ASSERT_TRUE(actual_value->GetAsList(&actual_encodings_list));
      std::vector<std::string> actual_encodings_vector;
      for (base::ListValue::const_iterator it = actual_encodings_list->begin();
           it != actual_encodings_list->end(); ++it) {
        std::string encoding;
        ASSERT_TRUE((*it)->GetAsString(&encoding));
        actual_encodings_vector.push_back(encoding);
      }
      EXPECT_EQ(expected_encodings, JoinString(actual_encodings_vector, ';'));
    } else {
      // Everything else is the same format.
      EXPECT_TRUE(actual_value->Equals(expected_value));
    }
  }
}

// If |simulate_failure| is false, then replies to the pending request on
// |fetcher| with a brandcoded config that only specifies a home page URL.
// If |simulate_failure| is true, replies with 404.
void ServicePendingBrancodedConfigFetch(net::TestURLFetcher* fetcher,
                                        bool simulate_failure) {
  const char kBrandcodedXmlSettings[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<response protocol=\"3.0\" server=\"prod\">"
        "<app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
          "<data index=\"skipfirstrunui-importsearch-defaultbrowser\" "
          "name=\"install\" status=\"ok\">"
            "{\"homepage\" : \"$1\"}"
          "</data>"
        "</app>"
      "</response>";

  fetcher->set_response_code(simulate_failure ? 404 : 200);
  scoped_refptr<net::HttpResponseHeaders> response_headers(
      new net::HttpResponseHeaders(""));
  response_headers->AddHeader("Content-Type: text/xml");
  fetcher->set_response_headers(response_headers);
  if (!simulate_failure) {
    std::string response(kBrandcodedXmlSettings);
    size_t placeholder_index = response.find("$1");
    ASSERT_NE(std::string::npos, placeholder_index);
    response.replace(placeholder_index, 2, kTestBrandedHomepage);
    fetcher->SetResponseString(response);
  }
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}


// Test fixture --------------------------------------------------------------

// ExtensionServiceTestBase sets up a TestingProfile with the ExtensionService,
// we then add the TemplateURLService, so the ProfileResetter can be exercised.
class AutomaticProfileResetterDelegateTest
    : public ExtensionServiceTestBase,
      public TemplateURLServiceTestUtilBase {
 protected:
  AutomaticProfileResetterDelegateTest() {}
  virtual ~AutomaticProfileResetterDelegateTest() {}

  virtual void SetUp() OVERRIDE {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceInitParams params = CreateDefaultInitParams();
    params.pref_file.clear();  // Prescribes a TestingPrefService to be created.
    InitializeExtensionService(params);
    TemplateURLServiceTestUtilBase::CreateTemplateUrlService();
    resetter_delegate_.reset(
        new AutomaticProfileResetterDelegateUnderTest(profile()));
  }

  virtual void TearDown() OVERRIDE {
    resetter_delegate_.reset();
    ExtensionServiceTestBase::TearDown();
  }

  scoped_ptr<TemplateURL> CreateTestTemplateURL() {
    TemplateURLData data;

    data.SetURL("http://example.com/search?q={searchTerms}");
    data.suggestions_url = "http://example.com/suggest?q={searchTerms}";
    data.instant_url = "http://example.com/instant?q={searchTerms}";
    data.image_url = "http://example.com/image?q={searchTerms}";
    data.search_url_post_params = "search-post-params";
    data.suggestions_url_post_params = "suggest-post-params";
    data.instant_url_post_params = "instant-post-params";
    data.image_url_post_params = "image-post-params";

    data.favicon_url = GURL("http://example.com/favicon.ico");
    data.new_tab_url = "http://example.com/newtab.html";
    data.alternate_urls.push_back("http://example.com/s?q={searchTerms}");

    data.short_name = base::ASCIIToUTF16("name");
    data.SetKeyword(base::ASCIIToUTF16("keyword"));
    data.search_terms_replacement_key = "search-terms-replacment-key";
    data.prepopulate_id = 42;
    data.input_encodings.push_back("UTF-8");
    data.safe_for_autoreplace = true;

    return scoped_ptr<TemplateURL>(new TemplateURL(profile(), data));
  }

  void ExpectNoPendingBrandcodedConfigFetch() {
    EXPECT_FALSE(test_url_fetcher_factory_.GetFetcherByID(0));
  }

  void ExpectAndServicePendingBrandcodedConfigFetch(bool simulate_failure) {
    net::TestURLFetcher* fetcher = test_url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    EXPECT_THAT(fetcher->upload_data(),
                testing::HasSubstr(kTestBrandcode));
    ServicePendingBrancodedConfigFetch(fetcher, simulate_failure);
  }

  void ExpectResetPromptState(bool active) {
    GlobalErrorService* global_error_service =
        GlobalErrorServiceFactory::GetForProfile(profile());
    GlobalError* global_error = global_error_service->
        GetGlobalErrorByMenuItemCommandID(IDC_SHOW_SETTINGS_RESET_BUBBLE);
    EXPECT_EQ(active, !!global_error);
  }

  AutomaticProfileResetterDelegateUnderTest* resetter_delegate() {
    return resetter_delegate_.get();
  }

  // TemplateURLServiceTestUtilBase:
  virtual TestingProfile* profile() const OVERRIDE { return profile_.get(); }

 private:
  net::TestURLFetcherFactory test_url_fetcher_factory_;
  scoped_ptr<AutomaticProfileResetterDelegateUnderTest> resetter_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AutomaticProfileResetterDelegateTest);
};


// Tests ---------------------------------------------------------------------

TEST_F(AutomaticProfileResetterDelegateTest,
       TriggerAndWaitOnModuleEnumeration) {
  // Expect ready_callback to be called just after the modules have been
  // enumerated. Fail if it is not called. Note: as the EnumerateModulesModel is
  // a global singleton, the callback might be invoked immediately if another
  // test-case (e.g. the one below) has already performed module enumeration.
  testing::StrictMock<MockCallbackTarget> mock_target;
  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->RequestCallbackWhenLoadedModulesAreEnumerated(
      mock_target.CreateClosure());
  resetter_delegate()->EnumerateLoadedModulesIfNeeded();
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_target);

  // Expect ready_callback to be posted immediately when the modules have
  // already been enumerated.
  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->RequestCallbackWhenLoadedModulesAreEnumerated(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

#if defined(OS_WIN)
  testing::Mock::VerifyAndClearExpectations(&mock_target);

  // Expect ready_callback to be posted immediately even when the modules had
  // already been enumerated when the delegate was constructed.
  scoped_ptr<AutomaticProfileResetterDelegate> late_resetter_delegate(
      new AutomaticProfileResetterDelegateImpl(profile(),
                                               ProfileResetter::ALL));

  EXPECT_CALL(mock_target, Run());
  late_resetter_delegate->RequestCallbackWhenLoadedModulesAreEnumerated(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();
#endif
}

TEST_F(AutomaticProfileResetterDelegateTest, GetLoadedModuleNameDigests) {
  resetter_delegate()->EnumerateLoadedModulesIfNeeded();
  base::RunLoop().RunUntilIdle();
  scoped_ptr<base::ListValue> module_name_digests(
      resetter_delegate()->GetLoadedModuleNameDigests());

  // Just verify that each element looks like an MD5 hash in hexadecimal, and
  // also that we have at least one element on Win.
  ASSERT_TRUE(module_name_digests);
  for (base::ListValue::const_iterator it = module_name_digests->begin();
       it != module_name_digests->end(); ++it) {
    std::string digest_hex;
    std::vector<uint8> digest_raw;

    ASSERT_TRUE((*it)->GetAsString(&digest_hex));
    ASSERT_TRUE(base::HexStringToBytes(digest_hex, &digest_raw));
    EXPECT_EQ(16u, digest_raw.size());
  }
#if defined(OS_WIN)
  EXPECT_LE(1u, module_name_digests->GetSize());
#endif
}

TEST_F(AutomaticProfileResetterDelegateTest, LoadAndWaitOnTemplateURLService) {
  // Expect ready_callback to be called just after the template URL service gets
  // initialized. Fail if it is not called, or called too early.
  testing::StrictMock<MockCallbackTarget> mock_target;
  resetter_delegate()->RequestCallbackWhenTemplateURLServiceIsLoaded(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->LoadTemplateURLServiceIfNeeded();
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_target);

  // Expect ready_callback to be posted immediately when the template URL
  // service is already initialized.
  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->RequestCallbackWhenTemplateURLServiceIsLoaded(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_target);

  // Expect ready_callback to be posted immediately even when the template URL
  // service had already been initialized when the delegate was constructed.
  scoped_ptr<AutomaticProfileResetterDelegate> late_resetter_delegate(
      new AutomaticProfileResetterDelegateImpl(profile(),
                                               ProfileResetter::ALL));

  EXPECT_CALL(mock_target, Run());
  late_resetter_delegate->RequestCallbackWhenTemplateURLServiceIsLoaded(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutomaticProfileResetterDelegateTest,
       DefaultSearchProviderDataWhenNotManaged) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLServiceTestUtilBase::VerifyLoad();

  // Check that the "managed state" and the details returned by the delegate are
  // correct. We verify the details against the data stored by
  // TemplateURLService into Prefs.
  scoped_ptr<TemplateURL> owned_custom_dsp(CreateTestTemplateURL());
  TemplateURL* custom_dsp = owned_custom_dsp.get();
  template_url_service->Add(owned_custom_dsp.release());
  template_url_service->SetDefaultSearchProvider(custom_dsp);

  PrefService* prefs = profile()->GetPrefs();
  ASSERT_TRUE(prefs);
  scoped_ptr<base::DictionaryValue> dsp_details(
      resetter_delegate()->GetDefaultSearchProviderDetails());
  scoped_ptr<base::DictionaryValue> expected_dsp_details(
      GetDefaultSearchProviderDetailsFromPrefs(prefs));

  ExpectDetailsMatch(*expected_dsp_details, *dsp_details);
  EXPECT_FALSE(resetter_delegate()->IsDefaultSearchProviderManaged());
}

TEST_F(AutomaticProfileResetterDelegateTest,
       DefaultSearchProviderDataWhenManaged) {
  const char kTestSearchURL[] = "http://example.com/search?q={searchTerms}";
  const char kTestName[] = "name";
  const char kTestKeyword[] = "keyword";

  TemplateURLServiceTestUtilBase::VerifyLoad();

  EXPECT_FALSE(resetter_delegate()->IsDefaultSearchProviderManaged());

  // Set managed preferences to emulate a default search provider set by policy.
  SetManagedDefaultSearchPreferences(
      true, kTestName, kTestKeyword, kTestSearchURL, std::string(),
      std::string(), std::string(), std::string(), std::string());

  EXPECT_TRUE(resetter_delegate()->IsDefaultSearchProviderManaged());
  scoped_ptr<base::DictionaryValue> dsp_details(
      resetter_delegate()->GetDefaultSearchProviderDetails());
  // Checking that all details are correct is already done by the above test.
  // Just make sure details are reported about the correct engine.
  base::ExpectDictStringValue(kTestSearchURL, *dsp_details, "search_url");

  // Set managed preferences to emulate that having a default search provider is
  // disabled by policy.
  RemoveManagedDefaultSearchPreferences();
  SetManagedDefaultSearchPreferences(
      true, std::string(), std::string(), std::string(), std::string(),
      std::string(), std::string(), std::string(), std::string());

  dsp_details = resetter_delegate()->GetDefaultSearchProviderDetails();
  EXPECT_TRUE(resetter_delegate()->IsDefaultSearchProviderManaged());
  EXPECT_TRUE(dsp_details->empty());
}

TEST_F(AutomaticProfileResetterDelegateTest,
       GetPrepopulatedSearchProvidersDetails) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLServiceTestUtilBase::VerifyLoad();

  scoped_ptr<base::ListValue> search_engines_details(
      resetter_delegate()->GetPrepopulatedSearchProvidersDetails());

  // Do the same kind of verification as for GetDefaultSearchEngineDetails:
  // subsequently set each pre-populated engine as the default, so we can verify
  // that the details returned by the delegate about one particular engine are
  // correct in comparison to what has been stored to the Prefs.
  std::vector<TemplateURL*> prepopulated_engines =
      template_url_service->GetTemplateURLs();

  ASSERT_EQ(prepopulated_engines.size(), search_engines_details->GetSize());

  for (size_t i = 0; i < search_engines_details->GetSize(); ++i) {
    const base::DictionaryValue* details = NULL;
    ASSERT_TRUE(search_engines_details->GetDictionary(i, &details));

    std::string keyword;
    ASSERT_TRUE(details->GetString("keyword", &keyword));
    TemplateURL* search_engine =
        template_url_service->GetTemplateURLForKeyword(ASCIIToUTF16(keyword));
    ASSERT_TRUE(search_engine);
    template_url_service->SetDefaultSearchProvider(prepopulated_engines[i]);

    PrefService* prefs = profile()->GetPrefs();
    ASSERT_TRUE(prefs);
    scoped_ptr<base::DictionaryValue> expected_dsp_details(
        GetDefaultSearchProviderDetailsFromPrefs(prefs));
    ExpectDetailsMatch(*expected_dsp_details, *details);
  }
}

TEST_F(AutomaticProfileResetterDelegateTest,
       FetchAndWaitOnDefaultSettingsVanilla) {
  google_util::BrandForTesting scoped_brand_for_testing((std::string()));

  // Expect ready_callback to be called just after empty brandcoded settings
  // are loaded, given this is a vanilla build. Fail if it is not called, or
  // called too early.
  testing::StrictMock<MockCallbackTarget> mock_target;
  resetter_delegate()->RequestCallbackWhenBrandcodedDefaultsAreFetched(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(resetter_delegate()->brandcoded_defaults());

  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->FetchBrandcodedDefaultSettingsIfNeeded();
  base::RunLoop().RunUntilIdle();
  ExpectNoPendingBrandcodedConfigFetch();

  testing::Mock::VerifyAndClearExpectations(&mock_target);
  EXPECT_TRUE(resetter_delegate()->brandcoded_defaults());

  // Expect ready_callback to be posted immediately when the brandcoded settings
  // have already been loaded.
  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->RequestCallbackWhenBrandcodedDefaultsAreFetched(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

  // No test for a new instance of AutomaticProfileResetterDelegate. That will
  // need to fetch the brandcoded settings again.
}

TEST_F(AutomaticProfileResetterDelegateTest,
       FetchAndWaitOnDefaultSettingsBranded) {
  google_util::BrandForTesting scoped_brand_for_testing(kTestBrandcode);

  // Expect ready_callback to be called just after the brandcoded settings are
  // downloaded. Fail if it is not called, or called too early.
  testing::StrictMock<MockCallbackTarget> mock_target;
  resetter_delegate()->RequestCallbackWhenBrandcodedDefaultsAreFetched(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(resetter_delegate()->brandcoded_defaults());

  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->FetchBrandcodedDefaultSettingsIfNeeded();
  ExpectAndServicePendingBrandcodedConfigFetch(false /*simulate_failure*/);
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_target);
  const BrandcodedDefaultSettings* brandcoded_defaults =
      resetter_delegate()->brandcoded_defaults();
  ASSERT_TRUE(brandcoded_defaults);
  std::string homepage_url;
  EXPECT_TRUE(brandcoded_defaults->GetHomepage(&homepage_url));
  EXPECT_EQ(kTestBrandedHomepage, homepage_url);

  // Expect ready_callback to be posted immediately when the brandcoded settings
  // have already been downloaded.
  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->RequestCallbackWhenBrandcodedDefaultsAreFetched(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutomaticProfileResetterDelegateTest,
       FetchAndWaitOnDefaultSettingsBrandedFailure) {
  google_util::BrandForTesting scoped_brand_for_testing(kTestBrandcode);

  // Expect ready_callback to be called just after the brandcoded settings have
  // failed to download. Fail if it is not called, or called too early.
  testing::StrictMock<MockCallbackTarget> mock_target;
  resetter_delegate()->RequestCallbackWhenBrandcodedDefaultsAreFetched(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->FetchBrandcodedDefaultSettingsIfNeeded();
  ExpectAndServicePendingBrandcodedConfigFetch(true /*simulate_failure*/);
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&mock_target);
  EXPECT_TRUE(resetter_delegate()->brandcoded_defaults());

  // Expect ready_callback to be posted immediately when the brandcoded settings
  // have already been attempted to be downloaded, but failed.
  EXPECT_CALL(mock_target, Run());
  resetter_delegate()->RequestCallbackWhenBrandcodedDefaultsAreFetched(
      mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutomaticProfileResetterDelegateTest, TriggerReset) {
  google_util::BrandForTesting scoped_brand_for_testing(kTestBrandcode);

  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kHomePage, kTestHomepage);

  testing::StrictMock<MockCallbackTarget> mock_target;
  EXPECT_CALL(mock_target, Run());
  EXPECT_CALL(*resetter_delegate(), SendFeedback(testing::_)).Times(0);
  resetter_delegate()->TriggerProfileSettingsReset(
      false /*send_feedback*/, mock_target.CreateClosure());
  ExpectAndServicePendingBrandcodedConfigFetch(false /*simulate_failure*/);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTestBrandedHomepage, prefs->GetString(prefs::kHomePage));
}

TEST_F(AutomaticProfileResetterDelegateTest,
       TriggerResetWithDefaultSettingsAlreadyLoaded) {
  google_util::BrandForTesting scoped_brand_for_testing(kTestBrandcode);

  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kHomePage, kTestHomepage);

  resetter_delegate()->FetchBrandcodedDefaultSettingsIfNeeded();
  ExpectAndServicePendingBrandcodedConfigFetch(false /*simulate_failure*/);
  base::RunLoop().RunUntilIdle();

  testing::StrictMock<MockCallbackTarget> mock_target;
  EXPECT_CALL(mock_target, Run());
  EXPECT_CALL(*resetter_delegate(), SendFeedback(testing::_)).Times(0);
  resetter_delegate()->TriggerProfileSettingsReset(
      false /*send_feedback*/, mock_target.CreateClosure());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTestBrandedHomepage, prefs->GetString(prefs::kHomePage));
}

TEST_F(AutomaticProfileResetterDelegateTest,
       TriggerResetAndSendFeedback) {
  google_util::BrandForTesting scoped_brand_for_testing(kTestBrandcode);

  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kHomePage, kTestHomepage);

  testing::StrictMock<MockCallbackTarget> mock_target;
  EXPECT_CALL(mock_target, Run());
  EXPECT_CALL(*resetter_delegate(),
              SendFeedback(testing::HasSubstr(kTestHomepage)));

  resetter_delegate()->TriggerProfileSettingsReset(
      true /*send_feedback*/, mock_target.CreateClosure());
  ExpectAndServicePendingBrandcodedConfigFetch(false /*simulate_failure*/);
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutomaticProfileResetterDelegateTest, ShowAndDismissPrompt) {
  resetter_delegate()->TriggerPrompt();
  if (ProfileResetGlobalError::IsSupportedOnPlatform())
    ExpectResetPromptState(true /*active*/);
  else
    ExpectResetPromptState(false /*active*/);
  resetter_delegate()->DismissPrompt();
  ExpectResetPromptState(false /*active*/);
  resetter_delegate()->DismissPrompt();
}

}  // namespace
