// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// Runs a tab-modal dialog that asks the user for a password.
class PDFPasswordDialogViews : public views::DialogDelegate {
 public:
  PDFPasswordDialogViews(content::WebContents* web_contents,
                         const base::string16& prompt,
                         const pdf::PasswordDialogClosedCallback& callback);
  ~PDFPasswordDialogViews() override;

  // views::DialogDelegate:
  base::string16 GetWindowTitle() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool Cancel() override;
  bool Accept() override;

  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;

 private:
  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  pdf::PasswordDialogClosedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PDFPasswordDialogViews);
};

PDFPasswordDialogViews::PDFPasswordDialogViews(
    content::WebContents* web_contents,
    const base::string16& prompt,
    const pdf::PasswordDialogClosedCallback& callback)
    : message_box_view_(NULL), callback_(callback) {
  views::MessageBoxView::InitParams init_params(prompt);
  init_params.options = views::MessageBoxView::HAS_PROMPT_FIELD;
  init_params.inter_row_vertical_spacing =
      views::kUnrelatedControlVerticalSpacing;
  message_box_view_ = new views::MessageBoxView(init_params);
  message_box_view_->text_box()->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  constrained_window::ShowWebModalDialogViews(this, web_contents);
}

PDFPasswordDialogViews::~PDFPasswordDialogViews() {
  if (!callback_.is_null()) {
    // This dialog was torn down without either OK or cancel being clicked; be
    // considerate and at least do the callback.
    callback_.Run(false, base::string16());
  }
}

//////////////////////////////////////////////////////////////////////////////
// PDFPasswordDialogViews, views::DialogDelegate implementation:

base::string16 PDFPasswordDialogViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_PDF_PASSWORD_DIALOG_TITLE);
}

base::string16 PDFPasswordDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_OK);
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return l10n_util::GetStringUTF16(IDS_CANCEL);
  return base::string16();
}

bool PDFPasswordDialogViews::Cancel() {
  callback_.Run(false, base::string16());
  callback_.Reset();
  return true;
}

bool PDFPasswordDialogViews::Accept() {
  callback_.Run(true, message_box_view_->text_box()->text());
  callback_.Reset();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// PDFPasswordDialogViews, views::WidgetDelegate implementation:

views::View* PDFPasswordDialogViews::GetInitiallyFocusedView() {
  return message_box_view_->text_box();
}

views::View* PDFPasswordDialogViews::GetContentsView() {
  return message_box_view_;
}

views::Widget* PDFPasswordDialogViews::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* PDFPasswordDialogViews::GetWidget() const {
  return message_box_view_->GetWidget();
}

void PDFPasswordDialogViews::DeleteDelegate() {
  delete this;
}

ui::ModalType PDFPasswordDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

}  // namespace

void ShowPDFPasswordDialog(content::WebContents* web_contents,
                           const base::string16& prompt,
                           const pdf::PasswordDialogClosedCallback& callback) {
  new PDFPasswordDialogViews(web_contents, prompt, callback);
}
