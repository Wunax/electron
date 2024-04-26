// Copyright (c) 2014 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "shell/browser/native_window_views.h"
#include "shell/browser/ui/file_dialog.h"
#include "shell/browser/ui/select_file_policy.h"
#include "shell/common/gin_converters/callback_converter.h"
#include "shell/common/gin_converters/file_path_converter.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace file_dialog {

DialogSettings::DialogSettings() = default;
DialogSettings::DialogSettings(const DialogSettings&) = default;
DialogSettings::~DialogSettings() = default;

namespace {

class FileChooserDialog : public ui::SelectFileDialog::Listener {
 public:
  enum class DialogType { OPEN, SAVE };

  FileChooserDialog() {
    dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ElectronSelectFilePolicy>(nullptr));
  }

  ~FileChooserDialog() override {
    // Clean up dialog_...
  }

  void RunSaveAsynchronous(gin_helper::Promise<gin_helper::Dictionary> promise,
                           const DialogSettings& settings) {
    promise_ = std::make_unique<gin_helper::Promise<gin_helper::Dictionary>>(
        std::move(promise));
    type_ = DialogType::SAVE;
    dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_SAVEAS_FILE,
        base::UTF8ToUTF16(settings.title), settings.default_path,
        nullptr /* file_types */, 0 /* file_type_index */,
        base::FilePath::StringType() /* default_extension */,
        settings.parent_window->GetNativeWindow(), nullptr /* params */);
  }

  void RunOpenAsynchronous(gin_helper::Promise<gin_helper::Dictionary> promise,
                           const DialogSettings& settings) {
    promise_ = std::make_unique<gin_helper::Promise<gin_helper::Dictionary>>(
        std::move(promise));
    type_ = DialogType::OPEN;
    dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_OPEN_FILE,
        base::UTF8ToUTF16(settings.title), settings.default_path,
        nullptr /* file_types */, 0 /* file_type_index */,
        base::FilePath::StringType() /* default_extension */,
        settings.parent_window->GetNativeWindow(), nullptr /* params */);
  }

  void FileSelected(const ui::SelectedFileInfo& file,
                    int index,
                    void* params) override {
    v8::HandleScope scope(promise_->isolate());
    auto dict = gin_helper::Dictionary::CreateEmpty(promise_->isolate());
    dict.Set("canceled", false);
    if (type_ == DialogType::SAVE) {
      dict.Set("filePath", file.file_path);
    } else {
      dict.Set("filePaths", std::vector<base::FilePath>{file.file_path});
    }

    promise_->Resolve(dict);
  }

  void MultiFilesSelected(const std::vector<ui::SelectedFileInfo>& files,
                          void* params) override {
    v8::HandleScope scope(promise_->isolate());
    auto dict = gin_helper::Dictionary::CreateEmpty(promise_->isolate());
    dict.Set("canceled", false);
    dict.Set("filePaths", ui::SelectedFileInfoListToFilePathList(files));
    promise_->Resolve(dict);
  }

  void FileSelectionCanceled(void* params) override {
    v8::HandleScope scope(promise_->isolate());
    auto dict = gin_helper::Dictionary::CreateEmpty(promise_->isolate());
    dict.Set("canceled", true);
    if (type_ == DialogType::SAVE) {
      dict.Set("filePath", base::FilePath());
    } else {
      dict.Set("filePaths", std::vector<base::FilePath>());
    }
    promise_->Resolve(dict);
  }

 private:
  DialogType type_;
  scoped_refptr<ui::SelectFileDialog> dialog_;
  std::unique_ptr<gin_helper::Promise<gin_helper::Dictionary>> promise_;
};

}  // namespace

bool ShowOpenDialogSync(const DialogSettings& settings,
                        std::vector<base::FilePath>* paths) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  gin_helper::Promise<gin_helper::Dictionary> promise;
  promise.Then(base::BindOnce(
      [](base::RunLoop* loop, std::vector<base::FilePath>* paths,
         gin_helper::Dictionary result) {
        result.Get("filePaths", paths);
        loop->Quit();
      },
      &run_loop, paths));

  FileChooserDialog* dialog = new FileChooserDialog();
  dialog->RunSaveAsynchronous(std::move(promise), settings);
  run_loop.Run();
  return paths->size() > 0;
}

void ShowOpenDialog(const DialogSettings& settings,
                    gin_helper::Promise<gin_helper::Dictionary> promise) {
  FileChooserDialog* dialog = new FileChooserDialog();
  dialog->RunOpenAsynchronous(std::move(promise), settings);
}

bool ShowSaveDialogSync(const DialogSettings& settings, base::FilePath* path) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  gin_helper::Promise<gin_helper::Dictionary> promise;
  promise.Then(base::BindOnce(
      [](base::RunLoop* loop, base::FilePath* path,
         gin_helper::Dictionary result) {
        result.Get("filePath", path);
        loop->Quit();
      },
      &run_loop, path));

  FileChooserDialog* dialog = new FileChooserDialog();
  dialog->RunSaveAsynchronous(std::move(promise), settings);
  run_loop.Run();
  return !path->empty();
}

void ShowSaveDialog(const DialogSettings& settings,
                    gin_helper::Promise<gin_helper::Dictionary> promise) {
  FileChooserDialog* dialog = new FileChooserDialog();
  dialog->RunSaveAsynchronous(std::move(promise), settings);
}

}  // namespace file_dialog
