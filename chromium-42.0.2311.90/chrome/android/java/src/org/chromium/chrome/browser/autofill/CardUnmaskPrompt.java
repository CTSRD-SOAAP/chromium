// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.ColorFilter;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.os.Build;
import android.os.Handler;
import android.support.v4.view.ViewCompat;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.chrome.R;

import java.util.Calendar;

/**
 * A prompt that bugs users to enter their CVC when unmasking a Wallet instrument (credit card).
 */
public class CardUnmaskPrompt implements DialogInterface.OnDismissListener, TextWatcher {
    private final CardUnmaskPromptDelegate mDelegate;
    private final AlertDialog mDialog;
    private final boolean mShouldRequestExpirationDate;
    private final int mThisYear;

    private final EditText mCardUnmaskInput;
    private final EditText mMonthInput;
    private final EditText mYearInput;
    private final View mExpirationContainer;
    private final TextView mErrorMessage;
    private final CheckBox mStoreLocallyCheckbox;
    private final View mMainContents;
    private final View mVerificationOverlay;
    private final ProgressBar mVerificationProgressBar;
    private final TextView mVerificationView;

    /**
     * An interface to handle the interaction with an CardUnmaskPrompt object.
     */
    public interface CardUnmaskPromptDelegate {
        /**
         * Called when the dialog has been dismissed.
         */
        void dismissed();

        /**
         * Returns whether |userResponse| represents a valid value.
         */
        boolean checkUserInputValidity(String userResponse);

        /**
         * Called when the user has entered a value and pressed "verify".
         * @param userResponse The value the user entered (a CVC), or an empty string if the
         *        user canceled.
         * @param month The value the user selected for expiration month, if any.
         * @param year The value the user selected for expiration month, if any.
         * @param shouldStoreLocally The state of the "Save locally?" checkbox at the time.
         */
        void onUserInput(String cvc, String month, String year, boolean shouldStoreLocally);
    }

    public CardUnmaskPrompt(Context context, CardUnmaskPromptDelegate delegate, String title,
            String instructions, int drawableId, boolean shouldRequestExpirationDate,
            boolean defaultToStoringLocally) {
        mDelegate = delegate;

        LayoutInflater inflater = LayoutInflater.from(context);
        View v = inflater.inflate(R.layout.autofill_card_unmask_prompt, null);
        ((TextView) v.findViewById(R.id.instructions)).setText(instructions);

        mCardUnmaskInput = (EditText) v.findViewById(R.id.card_unmask_input);
        mMonthInput = (EditText) v.findViewById(R.id.expiration_month);
        mYearInput = (EditText) v.findViewById(R.id.expiration_year);
        mExpirationContainer = v.findViewById(R.id.expiration_container);
        mErrorMessage = (TextView) v.findViewById(R.id.error_message);
        mStoreLocallyCheckbox = (CheckBox) v.findViewById(R.id.store_locally_checkbox);
        mStoreLocallyCheckbox.setChecked(defaultToStoringLocally);
        mMainContents = v.findViewById(R.id.main_contents);
        mVerificationOverlay = v.findViewById(R.id.verification_overlay);
        mVerificationProgressBar = (ProgressBar) v.findViewById(R.id.verification_progress_bar);
        mVerificationView = (TextView) v.findViewById(R.id.verification_message);
        ((ImageView) v.findViewById(R.id.cvc_hint_image)).setImageResource(drawableId);

        mDialog = new AlertDialog.Builder(context)
                          .setTitle(title)
                          .setView(v)
                          .setNegativeButton(R.string.cancel, null)
                          .setPositiveButton(R.string.card_unmask_confirm_button, null)
                          .create();
        mDialog.setOnDismissListener(this);

        mShouldRequestExpirationDate = shouldRequestExpirationDate;
        mThisYear = Calendar.getInstance().get(Calendar.YEAR);
    }

    public void show() {
        mDialog.show();

        if (mShouldRequestExpirationDate) mExpirationContainer.setVisibility(View.VISIBLE);

        // Override the View.OnClickListener so that pressing the positive button doesn't dismiss
        // the dialog.
        Button verifyButton = mDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        verifyButton.setEnabled(false);
        verifyButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                mDelegate.onUserInput(mCardUnmaskInput.getText().toString(),
                        mMonthInput.getText().toString(),
                        mYearInput.getText().toString(),
                        mStoreLocallyCheckbox.isChecked());
            }
        });

        mCardUnmaskInput.addTextChangedListener(this);
        mCardUnmaskInput.post(new Runnable() {
            @Override
            public void run() {
                setInitialFocus();
            }
        });
        if (mShouldRequestExpirationDate) {
            mMonthInput.addTextChangedListener(this);
            mYearInput.addTextChangedListener(this);
        }
    }

    public void dismiss() {
        mDialog.dismiss();
    }

    public void disableAndWaitForVerification() {
        setInputsEnabled(false);
        mVerificationProgressBar.setVisibility(View.VISIBLE);
        // TODO(estade): l10n
        mVerificationView.setText("Verifying card");
        setInputError(null);
    }

    public void verificationFinished(boolean success) {
        if (!success) {
            setInputsEnabled(true);
            setInputError("Credit card could not be verified. Try again.");
            // TODO(estade): depending on the type of error, we may not want to disable the
            // verify button. But for the common case, where unmasking failed due to a bad
            // value, verify should be disabled until the user makes some change.
            mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(false);
            setInitialFocus();
            // TODO(estade): UI decision - should we clear the input?
        } else {
            mVerificationProgressBar.setVisibility(View.GONE);
            mDialog.findViewById(R.id.verification_success).setVisibility(View.VISIBLE);
            mVerificationView.setText("Your card is verified");
            Handler h = new Handler();
            h.postDelayed(new Runnable() {
                @Override
                public void run() {
                    dismiss();
                }
            }, 500);
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        mDelegate.dismissed();
    }

    @Override
    public void afterTextChanged(Editable s) {
        mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(areInputsValid());
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {}

    private void setInitialFocus() {
        InputMethodManager imm = (InputMethodManager) mDialog.getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        imm.showSoftInput(mShouldRequestExpirationDate ? mMonthInput : mCardUnmaskInput,
                InputMethodManager.SHOW_IMPLICIT);
    }

    private boolean areInputsValid() {
        if (mShouldRequestExpirationDate) {
            try {
                int month = Integer.parseInt(mMonthInput.getText().toString());
                if (month < 1 || month > 12) return false;

                // TODO(estade): allow 4 digit year input?
                int year = Integer.parseInt(mYearInput.getText().toString());
                if (year < mThisYear % 100 || year > (mThisYear + 10) % 100) return false;
            } catch (NumberFormatException e) {
                return false;
            }
        }
        return mDelegate.checkUserInputValidity(mCardUnmaskInput.getText().toString());
    }

    /**
     * Sets the enabled state of the main contents, and hides or shows the verification overlay.
     * @param enabled True if the inputs should be useable, false if the verification overlay
     *        obscures them.
     */
    private void setInputsEnabled(boolean enabled) {
        mCardUnmaskInput.setEnabled(enabled);
        mMonthInput.setEnabled(enabled);
        mYearInput.setEnabled(enabled);
        mMainContents.setAlpha(enabled ? 1.0f : 0.15f);
        ViewCompat.setImportantForAccessibility(mMainContents,
                enabled ? View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
                        : View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        ((ViewGroup) mMainContents).setDescendantFocusability(
                enabled ? ViewGroup.FOCUS_BEFORE_DESCENDANTS
                        : ViewGroup.FOCUS_BLOCK_DESCENDANTS);
        mDialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(enabled);

        mVerificationOverlay.setVisibility(enabled ? View.GONE : View.VISIBLE);
    }

    /**
     * Sets the error message on the cvc input.
     * @param message The error message to show, or null if the error state should be cleared.
     */
    private void setInputError(String message) {
        mErrorMessage.setText(message);
        mErrorMessage.setVisibility(message == null ? View.GONE : View.VISIBLE);

        // The rest of this code makes L-specific assumptions about the background being used to
        // draw the TextInput.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        ColorFilter filter = null;
        if (message != null) {
            filter = new PorterDuffColorFilter(mDialog.getContext().getResources().getColor(
                    R.color.input_underline_error_color), PorterDuff.Mode.SRC_IN);
        }

        // TODO(estade): it would be nicer if the error were specific enough to tell us which input
        // was invalid.
        updateColorForInput(mCardUnmaskInput, filter);
        updateColorForInput(mMonthInput, filter);
        updateColorForInput(mYearInput, filter);
    }

    /**
     * Sets the stroke color for the given input.
     * @param input The input to modify.
     * @param filter The color filter to apply to the background.
     */
    private void updateColorForInput(EditText input, ColorFilter filter) {
        input.getBackground().mutate().setColorFilter(filter);
    }
}
