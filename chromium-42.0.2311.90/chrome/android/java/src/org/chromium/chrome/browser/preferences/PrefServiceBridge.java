// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;

import java.util.ArrayList;
import java.util.List;

/**
 * PrefServiceBridge is a singleton which provides access to some native preferences. Ideally
 * preferences should be grouped with their relevant functionality but this is a grab-bag for other
 * preferences.
 */
public final class PrefServiceBridge {

    // Does not need sync with native; used for the popup settings check
    public static final String EXCEPTION_SETTING_ALLOW = "allow";
    public static final String EXCEPTION_SETTING_BLOCK = "block";
    public static final String EXCEPTION_SETTING_DEFAULT = "default";

    // These values must match the native enum values in
    // SupervisedUserURLFilter::FilteringBehavior
    public static final int SUPERVISED_USER_FILTERING_ALLOW = 0;
    public static final int SUPERVISED_USER_FILTERING_WARN = 1;
    public static final int SUPERVISED_USER_FILTERING_BLOCK = 2;

    private static final String MIGRATION_PREF_KEY = "PrefMigrationVersion";
    private static final int MIGRATION_CURRENT_VERSION = 1;

    private static String sProfilePath;

    // Object to notify when "clear browsing data" completes.
    private OnClearBrowsingDataListener mClearBrowsingDataListener;
    private static final String LOG_TAG = "PrefServiceBridge";

    // Constants related to the Contextual Search preference.
    private static final String CONTEXTUAL_SEARCH_DISABLED = "false";
    private static final String CONTEXTUAL_SEARCH_ENABLED = "true";

    /**
     * Structure that holds all the version information about the current Chrome browser.
     */
    public static class AboutVersionStrings {
        private final String mApplicationVersion;
        private final String mWebkitVersion;
        private final String mJavascriptVersion;
        private final String mOSVersion;

        private AboutVersionStrings(String applicationVersion, String webkitVersion,
                String javascriptVersion, String osVersion) {
            mApplicationVersion = applicationVersion;
            mWebkitVersion = webkitVersion;
            mJavascriptVersion = javascriptVersion;
            mOSVersion = osVersion;
        }

        public String getApplicationVersion() {
            return mApplicationVersion;
        }

        public String getWebkitVersion() {
            return mWebkitVersion;
        }

        public String getJavascriptVersion() {
            return mJavascriptVersion;
        }

        public String getOSVersion() {
            return mOSVersion;
        }
    }

    /**
     * Callback to receive the profile path.
     */
    public interface ProfilePathCallback {
        /**
         * Called with the profile path, once it's available.
         */
        void onGotProfilePath(String profilePath);
    }

    /**
     * Website popup exception entry.
     */
    public static class PopupExceptionInfo {
        private final String mPattern;
        private final String mSetting;
        private final String mSource;

        private PopupExceptionInfo(String pattern, String setting, String source) {
            mPattern = pattern;
            mSetting = setting;
            mSource = source;
        }

        public String getPattern() {
            return mPattern;
        }

        public String getSetting() {
            return mSetting;
        }

        public String getSource() {
            return mSource;
        }
    }

    @CalledByNative
    private static AboutVersionStrings createAboutVersionStrings(
            String applicationVersion, String webkitVersion, String javascriptVersion,
            String osVersion) {
        return new AboutVersionStrings(
                applicationVersion, webkitVersion, javascriptVersion, osVersion);
    }

    private PrefServiceBridge() {
        TemplateUrlService.getInstance().load();
    }

    private static PrefServiceBridge sInstance;

    /**
     * @return The singleton preferences object.
     */
    public static PrefServiceBridge getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new PrefServiceBridge();
        return sInstance;
    }

    /**
     * @return Whether the preferences have been initialized.
     */
    public static boolean isInitialized() {
        return sInstance != null;
    }

    /**
     * Migrates (synchronously) the preferences to the most recent version.
     */
    public void migratePreferences(Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        int currentVersion = preferences.getInt(MIGRATION_PREF_KEY, 0);
        if (currentVersion == MIGRATION_CURRENT_VERSION) return;
        if (currentVersion > MIGRATION_CURRENT_VERSION) {
            Log.e(LOG_TAG, "Saved preferences version is newer than supported.  Attempting to "
                    + "run an older version of Chrome without clearing data is unsupported and "
                    + "the results may be unpredictable.");
        }

        if (currentVersion <= 0) {
            nativeMigrateJavascriptPreference();
        }
        preferences.edit().putInt(MIGRATION_PREF_KEY, MIGRATION_CURRENT_VERSION).commit();
    }

    /**
     * Returns the path to the user's profile directory via a callback. The callback may be
     * called synchronously or asynchronously.
     */
    public void getProfilePath(ProfilePathCallback callback) {
        if (!TextUtils.isEmpty(sProfilePath)) {
            callback.onGotProfilePath(sProfilePath);
        } else {
            nativeGetProfilePath(callback);
        }
    }

    @CalledByNative
    private static void onGotProfilePath(String profilePath, ProfilePathCallback callback) {
        sProfilePath = profilePath;
        callback.onGotProfilePath(profilePath);
    }

    public boolean isAcceptCookiesEnabled() {
        return nativeGetAcceptCookiesEnabled();
    }

    /**
     * @return whether cookies acceptance is configured by policy
     */
    public boolean isAcceptCookiesManaged() {
        return nativeGetAcceptCookiesManaged();
    }

    public boolean isBlockThirdPartyCookiesEnabled() {
        return nativeGetBlockThirdPartyCookiesEnabled();
    }

    /**
     * @return whether third-party cookie blocking is configured by policy
     */
    public boolean isBlockThirdPartyCookiesManaged() {
        return nativeGetBlockThirdPartyCookiesManaged();
    }

    public boolean isRememberPasswordsEnabled() {
        return nativeGetRememberPasswordsEnabled();
    }

    /**
     * @return whether password storage is configured by policy
     */
    public boolean isRememberPasswordsManaged() {
        return nativeGetRememberPasswordsManaged();
    }

    /**
     * @return whether push notifications are enabled.
     */
    public boolean isPushNotificationsEnabled() {
        return nativeGetPushNotificationsEnabled();
    }

    /**
     * @return whether geolocation information can be shared with content
     */
    public boolean isAllowLocationEnabled() {
        return nativeGetAllowLocationEnabled();
    }

    /**
     * @return whether the location preference is modifiable by the user.
     */
    public boolean isAllowLocationUserModifiable() {
        return nativeGetAllowLocationUserModifiable();
    }

    /**
     * @return whether the location preference is
     * being managed by the custodian of the supervised account.
     */
    public boolean isAllowLocationManagedByCustodian() {
        return nativeGetAllowLocationManagedByCustodian();
    }

    /**
     * @return whether Do Not Track is enabled
     */
    public boolean isDoNotTrackEnabled() {
        return nativeGetDoNotTrackEnabled();
    }

    public boolean getPasswordEchoEnabled() {
        return nativeGetPasswordEchoEnabled();
    }

    /**
     * @return Whether EULA has been accepted by the user.
     */
    public boolean isFirstRunEulaAccepted() {
        return nativeGetFirstRunEulaAccepted();
    }

    /**
     * @return true if JavaScript is enabled. It may return the temporary value set by
     * {@link #setJavaScriptEnabled}. The default is true.
     */
    public boolean javaScriptEnabled() {
        return nativeGetJavaScriptEnabled();
    }

    /**
     * @return whether Javascript is managed by policy
     */
    public boolean javaScriptManaged() {
        return nativeGetJavaScriptManaged();
    }

    /**
     * Sets the preference that controls protected media identifier.
     */
    public void setProtectedMediaIdentifierEnabled(boolean enabled) {
        nativeSetProtectedMediaIdentifierEnabled(enabled);
    }

    /**
     * Sets the preference that controls translate
     */
    public void setTranslateEnabled(boolean enabled) {
        nativeSetTranslateEnabled(enabled);
    }

    /**
     * Sets the preference that signals when the user has accepted the EULA.
     */
    public void setEulaAccepted() {
        nativeSetEulaAccepted();
    }

    /**
     * Resets translate defaults if needed
     */
    public void resetTranslateDefaults() {
        nativeResetTranslateDefaults();
    }

    /**
     * Enable or disable JavaScript.
     */
    public void setJavaScriptEnabled(boolean enabled) {
        nativeSetJavaScriptEnabled(enabled);
    }

    /**
     * @return the last account username associated with sync.
     */
    public String getSyncLastAccountName() {
        return nativeGetSyncLastAccountName();
    }

    /**
     * Enable or disable x-auto-login
     */
    public void setAutologinEnabled(boolean autologinEnabled) {
        nativeSetAutologinEnabled(autologinEnabled);
    }

    /**
     * @return true if x-auto-login is enabled, false otherwise.
     */
    public boolean isAutologinEnabled() {
        return nativeGetAutologinEnabled();
    }

    /**
     * @return whether usage and crash report is managed.
     */
    public boolean isCrashReportManaged() {
        return nativeGetCrashReportManaged();
    }

    /**
     * Enable or disable crashes_ui.
     */
    public void setCrashReporting(boolean reporting) {
        nativeSetCrashReporting(reporting);
    }

    /**
     * @return whether Search Suggest is enabled.
     */
    public boolean isSearchSuggestEnabled() {
        return nativeGetSearchSuggestEnabled();
    }

    /**
     * Sets whether search suggest should be enabled.
     */
    public void setSearchSuggestEnabled(boolean enabled) {
        nativeSetSearchSuggestEnabled(enabled);
    }

    /**
     * @return whether Search Suggest is configured by policy.
     */
    public boolean isSearchSuggestManaged() {
        return nativeGetSearchSuggestManaged();
    }

    /**
     * @return the Contextual Search preference.
     */
    public String getContextualSearchPreference() {
        return nativeGetContextualSearchPreference();
    }

    /**
     * Sets the Contextual Search preference.
     * @param prefValue one of "", CONTEXTUAL_SEARCH_ENABLED or CONTEXTUAL_SEARCH_DISABLED.
     */
    public void setContextualSearchPreference(String prefValue) {
        nativeSetContextualSearchPreference(prefValue);
    }

    /**
     * @return whether the Contextual Search feature was disabled by the user explicitly.
     */
    public boolean isContextualSearchDisabled() {
        return getContextualSearchPreference().equals(CONTEXTUAL_SEARCH_DISABLED);
    }

    /**
     * @return whether the Contextual Search feature is disabled by policy.
     */
    public boolean isContextualSearchDisabledByPolicy() {
        return nativeGetContextualSearchPreferenceIsManaged()
                && isContextualSearchDisabled();
    }

    /**
     * @return whether the Contextual Search feature is uninitialized (preference unset by the
     *         user).
     */
    public boolean isContextualSearchUninitialized() {
        return getContextualSearchPreference().isEmpty();
    }

    /**
     * @param whether Contextual Search should be enabled.
     */
    public void setContextualSearchState(boolean enabled) {
        setContextualSearchPreference(enabled
                ? CONTEXTUAL_SEARCH_ENABLED : CONTEXTUAL_SEARCH_DISABLED);
    }

    /**
     * @return whether there is a user set value for kNetworkPredictionEnabled.  This should only be
     * used for preference migration.
     */
    public boolean networkPredictionEnabledHasUserSetting() {
        return nativeNetworkPredictionEnabledHasUserSetting();
    }

    /**
     * @return whether there is a user set value for kNetworkPredictionOptions.  This should only be
     * used for preference migration.
     */
    public boolean networkPredictionOptionsHasUserSetting() {
        return nativeNetworkPredictionOptionsHasUserSetting();
    }

    /**
     * @return the user set value for kNetworkPredictionEnabled. This should only be used for
     * preference migration.
     */
    public boolean getNetworkPredictionEnabledUserPrefValue() {
        return nativeGetNetworkPredictionEnabledUserPrefValue();
    }

    /**
     * @return Network predictions preference.
     */
    public NetworkPredictionOptions getNetworkPredictionOptions() {
        return NetworkPredictionOptions.intToEnum(nativeGetNetworkPredictionOptions());
    }

    /**
     * Sets network predictions preference.
     */
    public void setNetworkPredictionOptions(NetworkPredictionOptions option) {
        nativeSetNetworkPredictionOptions(option.enumToInt());
    }

    /**
     * @return whether Network Predictions is configured by policy.
     */
    public boolean isNetworkPredictionManaged() {
        return nativeGetNetworkPredictionManaged();
    }

    /**
     * Checks whether network predictions are allowed given preferences and current network
     * connection type.
     * @return Whether network predictions are allowed.
     */
    public boolean canPredictNetworkActions() {
        return nativeCanPredictNetworkActions();
    }

    /**
     * @return whether the web service to resolve navigation error is enabled.
     */
    public boolean isResolveNavigationErrorEnabled() {
        return nativeGetResolveNavigationErrorEnabled();
    }

    /**
     * @return whether the web service to resolve navigation error is configured by policy.
     */
    public boolean isResolveNavigationErrorManaged() {
        return nativeGetResolveNavigationErrorManaged();
    }

    /**
     * @return whether or not the protected media identifier is enabled.
     */
    public boolean isProtectedMediaIdentifierEnabled() {
        return nativeGetProtectedMediaIdentifierEnabled();
    }

    /**
     * @return true if translate is enabled, false otherwise.
     */
    public boolean isTranslateEnabled() {
        return nativeGetTranslateEnabled();
    }

    /**
     * @return whether translate is configured by policy
     */
    public boolean isTranslateManaged() {
        return nativeGetTranslateManaged();
    }

    /**
     * Sets whether the web service to resolve navigation error should be enabled.
     */
    public void setResolveNavigationErrorEnabled(boolean enabled) {
        nativeSetResolveNavigationErrorEnabled(enabled);
    }

    /**
     * Interface for a class that is listening to clear browser data events.
     */
    public interface OnClearBrowsingDataListener {
        public abstract void onBrowsingDataCleared();
    }

    /**
     * Clear the specified types of browsing data asynchronously.
     * |listener| is an object to be notified when clearing completes.
     * It can be null, but many operations (e.g. navigation) are
     * ill-advised while browsing data is being cleared.
     */
    public void clearBrowsingData(OnClearBrowsingDataListener listener,
            boolean history, boolean cache, boolean cookiesAndSiteData,
            boolean passwords, boolean formData) {
        assert mClearBrowsingDataListener == null;
        mClearBrowsingDataListener = listener;
        nativeClearBrowsingData(history, cache, cookiesAndSiteData, passwords, formData);
    }

    @CalledByNative
    private void browsingDataCleared() {
        if (mClearBrowsingDataListener != null) {
            mClearBrowsingDataListener.onBrowsingDataCleared();
            mClearBrowsingDataListener = null;
        }
    }

    public void setAllowCookiesEnabled(boolean allow) {
        nativeSetAllowCookiesEnabled(allow);
    }

    public void setBlockThirdPartyCookiesEnabled(boolean enabled) {
        nativeSetBlockThirdPartyCookiesEnabled(enabled);
    }

    public void setDoNotTrackEnabled(boolean enabled) {
        nativeSetDoNotTrackEnabled(enabled);
    }

    public void setRememberPasswordsEnabled(boolean allow) {
        nativeSetRememberPasswordsEnabled(allow);
    }

    public void setPushNotificationsEnabled(boolean allow) {
        nativeSetPushNotificationsEnabled(allow);
    }

    public void setAllowLocationEnabled(boolean allow) {
        nativeSetAllowLocationEnabled(allow);
    }

    public void setPasswordEchoEnabled(boolean enabled) {
        nativeSetPasswordEchoEnabled(enabled);
    }

    /**
     * @return The setting if popups are enabled
     */
    public boolean popupsEnabled() {
        return nativeGetAllowPopupsEnabled();
    }

    /**
     * @return Whether the setting to allow popups is configured by policy
     */
    public boolean isPopupsManaged() {
        return nativeGetAllowPopupsManaged();
    }

    /**
     * Sets the preferences on whether to enable/disable popups
     *
     * @param allow attribute to enable/disable popups
     */
    public void setAllowPopupsEnabled(boolean allow) {
        nativeSetAllowPopupsEnabled(allow);
    }

    /**
     * @return Whether the camera/microphone permission is enabled.
     */
    public boolean isCameraMicEnabled() {
        return nativeGetCameraMicEnabled();
    }

    /**
     * Sets the preferences on whether to enable/disable camera and microphone
     */
    public void setCameraMicEnabled(boolean enabled) {
        nativeSetCameraMicEnabled(enabled);
    }

    /**
     * @return Whether the camera/microphone permission is managed
     * by the custodian of the supervised account.
     */
    public boolean isCameraMicManagedByCustodian() {
        return nativeGetCameraMicManagedByCustodian();
    }

    /**
     * @return Whether the camera/microphone permission is editable by the user.
     */
    public boolean isCameraMicUserModifiable() {
        return nativeGetCameraMicUserModifiable();
    }

    /**
     * @return true if incognito mode is enabled.
     */
    public boolean isIncognitoModeEnabled() {
        return nativeGetIncognitoModeEnabled();
    }

    /**
     * @return true if incognito mode is managed by policy.
     */
    public boolean isIncognitoModeManaged() {
        return nativeGetIncognitoModeManaged();
    }

    /**
     * @return Whether printing is enabled.
     */
    public boolean isPrintingEnabled() {
        return nativeGetPrintingEnabled();
    }

    /**
     * @return Whether printing is managed by policy.
     */
    public boolean isPrintingManaged() {
        return nativeGetPrintingManaged();
    }

    /**
     * Adds/Edit a popup exception
     *
     * @param pattern attribute for the popup exception pattern
     * @param allow attribute to specify whether to allow or block pattern
     */
    public void setPopupException(String pattern, boolean allow) {
        nativeSetPopupException(pattern, allow);
    }

    /**
     * Removes a popup exception
     *
     * @param pattern attribute for the popup exception pattern
     */
    public void removePopupException(String pattern) {
        nativeRemovePopupException(pattern);
    }

    /**
     * get all the currently saved popup exceptions
     *
     * @return List of all the exceptions and their settings
     */
    public List<PopupExceptionInfo> getPopupExceptions() {
        List<PopupExceptionInfo> list = new ArrayList<PopupExceptionInfo>();
        nativeGetPopupExceptions(list);
        return list;
    }

    @CalledByNative
    private static void insertPopupExceptionToList(
            ArrayList<PopupExceptionInfo> list, String pattern, String setting, String source) {
        PopupExceptionInfo exception = new PopupExceptionInfo(pattern, setting, source);
        list.add(exception);
    }

    /**
     * Get all the version strings from native.
     * @return AboutVersionStrings about version strings.
     */
    public AboutVersionStrings getAboutVersionStrings() {
        return nativeGetAboutVersionStrings();
    }

    /**
     * Reset accept-languages to its default value.
     *
     * @param defaultLocale A fall-back value such as en_US, de_DE, zh_CN, etc.
     */
    public void resetAcceptLanguages(String defaultLocale) {
        nativeResetAcceptLanguages(defaultLocale);
    }

    /**
     * @return whether ForceSafeSearch is set
     */
    public boolean isForceSafeSearch() {
        return nativeGetForceSafeSearch();
    }

    /**
     * @return the default supervised user filtering behavior
     */
    public int getDefaultSupervisedUserFilteringBehavior() {
        return nativeGetDefaultSupervisedUserFilteringBehavior();
    }

    public String getSupervisedUserCustodianName() {
        return nativeGetSupervisedUserCustodianName();
    }

    public String getSupervisedUserCustodianEmail() {
        return nativeGetSupervisedUserCustodianEmail();
    }

    public String getSupervisedUserCustodianProfileImageURL() {
        return nativeGetSupervisedUserCustodianProfileImageURL();
    }

    public String getSupervisedUserSecondCustodianName() {
        return nativeGetSupervisedUserSecondCustodianName();
    }

    public String getSupervisedUserSecondCustodianEmail() {
        return nativeGetSupervisedUserSecondCustodianEmail();
    }

    public String getSupervisedUserSecondCustodianProfileImageURL() {
        return nativeGetSupervisedUserSecondCustodianProfileImageURL();
    }

    private native boolean nativeGetAcceptCookiesEnabled();
    private native boolean nativeGetAcceptCookiesManaged();
    private native boolean nativeGetBlockThirdPartyCookiesEnabled();
    private native boolean nativeGetBlockThirdPartyCookiesManaged();
    private native boolean nativeGetRememberPasswordsEnabled();
    private native boolean nativeGetRememberPasswordsManaged();
    private native boolean nativeGetAllowLocationUserModifiable();
    private native boolean nativeGetAllowLocationManagedByCustodian();
    private native boolean nativeGetDoNotTrackEnabled();
    private native boolean nativeGetPasswordEchoEnabled();
    private native boolean nativeGetFirstRunEulaAccepted();
    private native boolean nativeGetJavaScriptManaged();
    private native boolean nativeGetCameraMicUserModifiable();
    private native boolean nativeGetCameraMicManagedByCustodian();
    private native boolean nativeGetTranslateEnabled();
    private native boolean nativeGetTranslateManaged();
    private native boolean nativeGetResolveNavigationErrorEnabled();
    private native boolean nativeGetResolveNavigationErrorManaged();
    private native boolean nativeGetProtectedMediaIdentifierEnabled();
    private native boolean nativeGetCrashReportManaged();
    private native boolean nativeGetIncognitoModeEnabled();
    private native boolean nativeGetIncognitoModeManaged();
    private native boolean nativeGetPrintingEnabled();
    private native boolean nativeGetPrintingManaged();
    private native boolean nativeGetForceSafeSearch();
    private native void nativeSetTranslateEnabled(boolean enabled);
    private native void nativeResetTranslateDefaults();
    private native boolean nativeGetJavaScriptEnabled();
    private native void nativeSetJavaScriptEnabled(boolean enabled);
    private native void nativeMigrateJavascriptPreference();
    private native void nativeClearBrowsingData(boolean history, boolean cache,
            boolean cookiesAndSiteData, boolean passwords, boolean formData);
    private native void nativeSetAllowCookiesEnabled(boolean allow);
    private native void nativeSetBlockThirdPartyCookiesEnabled(boolean enabled);
    private native void nativeSetDoNotTrackEnabled(boolean enabled);
    private native void nativeSetRememberPasswordsEnabled(boolean allow);
    private native void nativeSetProtectedMediaIdentifierEnabled(boolean enabled);
    private native boolean nativeGetAllowLocationEnabled();
    private native boolean nativeGetPushNotificationsEnabled();
    private native void nativeSetAllowLocationEnabled(boolean allow);
    private native void nativeSetCameraMicEnabled(boolean allow);
    private native void nativeSetPushNotificationsEnabled(boolean allow);
    private native void nativeSetPasswordEchoEnabled(boolean enabled);
    private native boolean nativeGetAllowPopupsEnabled();
    private native boolean nativeGetAllowPopupsManaged();
    private native void nativeSetAllowPopupsEnabled(boolean allow);
    private native void nativeSetPopupException(String pattern, boolean allow);
    private native void nativeRemovePopupException(String pattern);
    private native void nativeGetPopupExceptions(Object list);
    private native boolean nativeGetCameraMicEnabled();
    private native boolean nativeGetAutologinEnabled();
    private native void nativeSetAutologinEnabled(boolean autologinEnabled);
    private native void nativeSetCrashReporting(boolean reporting);
    private native boolean nativeCanPredictNetworkActions();
    private native AboutVersionStrings nativeGetAboutVersionStrings();
    private native void nativeGetProfilePath(ProfilePathCallback callback);
    private native void nativeSetContextualSearchPreference(String preference);
    private native String nativeGetContextualSearchPreference();
    private native boolean nativeGetContextualSearchPreferenceIsManaged();
    private native boolean nativeGetSearchSuggestEnabled();
    private native void nativeSetSearchSuggestEnabled(boolean enabled);
    private native boolean nativeGetSearchSuggestManaged();
    private native boolean nativeGetNetworkPredictionManaged();
    private native boolean nativeNetworkPredictionEnabledHasUserSetting();
    private native boolean nativeNetworkPredictionOptionsHasUserSetting();
    private native boolean nativeGetNetworkPredictionEnabledUserPrefValue();
    private native int nativeGetNetworkPredictionOptions();
    private native void nativeSetNetworkPredictionOptions(int option);
    private native void nativeSetResolveNavigationErrorEnabled(boolean enabled);
    private native void nativeSetEulaAccepted();
    private native void nativeResetAcceptLanguages(String defaultLocale);
    private native String nativeGetSyncLastAccountName();
    private native String nativeGetSupervisedUserCustodianName();
    private native String nativeGetSupervisedUserCustodianEmail();
    private native String nativeGetSupervisedUserCustodianProfileImageURL();
    private native int nativeGetDefaultSupervisedUserFilteringBehavior();
    private native String nativeGetSupervisedUserSecondCustodianName();
    private native String nativeGetSupervisedUserSecondCustodianEmail();
    private native String nativeGetSupervisedUserSecondCustodianProfileImageURL();
}
