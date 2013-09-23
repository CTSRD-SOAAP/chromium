// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/volume_controller_chromeos.h"

#include "ash/ash_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/extensions/api/system_private/system_private_api.h"
#include "content/public/browser/user_metrics.h"

using chromeos::CrasAudioHandler;

namespace {

// Percent by which the volume should be changed when a volume key is pressed.
const double kStepPercentage = 4.0;

}  // namespace

VolumeController::VolumeController() {
  if (ash::switches::UseNewAudioHandler())
    CrasAudioHandler::Get()->AddAudioObserver(this);
}

VolumeController::~VolumeController() {
  if (ash::switches::UseNewAudioHandler() && CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

bool VolumeController::HandleVolumeMute(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_MUTE)
    content::RecordAction(content::UserMetricsAction("Accel_VolumeMute_F8"));

  if (ash::switches::UseNewAudioHandler()) {
    CrasAudioHandler::Get()->SetOutputMute(true);
    return true;
  }

  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();

  // Always muting (and not toggling) as per final decision on
  // http://crosbug.com/3751
  audio_handler->SetMuted(true);

  extensions::DispatchVolumeChangedEvent(audio_handler->GetVolumePercent(),
                                         audio_handler->IsMuted());
  return true;
}

bool VolumeController::HandleVolumeDown(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_DOWN)
    content::RecordAction(content::UserMetricsAction("Accel_VolumeDown_F9"));

  if (ash::switches::UseNewAudioHandler()) {
    CrasAudioHandler* audio_handler = CrasAudioHandler::Get();

    if (audio_handler->IsOutputMuted()) {
      audio_handler->SetOutputVolumePercent(0);
    } else {
      audio_handler->AdjustOutputVolumeByPercent(-kStepPercentage);
      if (audio_handler->IsOutputVolumeBelowDefaultMuteLvel())
        audio_handler->SetOutputMute(true);
    }
    return true;
  }

  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();
  if (audio_handler->IsMuted())
    audio_handler->SetVolumePercent(0.0);
  else
    audio_handler->AdjustVolumeByPercent(-kStepPercentage);

  extensions::DispatchVolumeChangedEvent(audio_handler->GetVolumePercent(),
                                         audio_handler->IsMuted());
  return true;
}

bool VolumeController::HandleVolumeUp(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_UP)
    content::RecordAction(content::UserMetricsAction("Accel_VolumeUp_F10"));

  if (ash::switches::UseNewAudioHandler()) {
    CrasAudioHandler* audio_handler = CrasAudioHandler::Get();

    if (audio_handler->IsOutputMuted())
      audio_handler->SetOutputMute(false);
    else
      audio_handler->AdjustOutputVolumeByPercent(kStepPercentage);
    return true;
  }

  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();
  if (audio_handler->IsMuted()) {
    audio_handler->SetMuted(false);
  } else {
    audio_handler->AdjustVolumeByPercent(kStepPercentage);
  }

  extensions::DispatchVolumeChangedEvent(audio_handler->GetVolumePercent(),
                                         audio_handler->IsMuted());
  return true;
}

bool VolumeController::IsAudioMuted() const {
  DCHECK(!ash::switches::UseNewAudioHandler());
  return chromeos::AudioHandler::GetInstance()->IsMuted();
}

void VolumeController::SetAudioMuted(bool muted) {
  DCHECK(!ash::switches::UseNewAudioHandler());
  chromeos::AudioHandler::GetInstance()->SetMuted(muted);
}

// Gets the volume level. The range is [0, 1.0].
float VolumeController::GetVolumeLevel() const {
  DCHECK(!ash::switches::UseNewAudioHandler());
  return chromeos::AudioHandler::GetInstance()->GetVolumePercent() / 100.f;
}

// Sets the volume level. The range is [0, 1.0].
void VolumeController::SetVolumeLevel(float level) {
  SetVolumePercent(level * 100.f);
}

void VolumeController::SetVolumePercent(double percent) {
  DCHECK(!ash::switches::UseNewAudioHandler());
  chromeos::AudioHandler* audio_handler = chromeos::AudioHandler::GetInstance();
  audio_handler->SetVolumePercent(percent);
  extensions::DispatchVolumeChangedEvent(audio_handler->GetVolumePercent(),
                                         audio_handler->IsMuted());
}

void VolumeController::OnOutputVolumeChanged() {
  DCHECK(ash::switches::UseNewAudioHandler());
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  extensions::DispatchVolumeChangedEvent(
      audio_handler->GetOutputVolumePercent(),
      audio_handler->IsOutputMuted());
}

void VolumeController::OnOutputMuteChanged() {
  DCHECK(ash::switches::UseNewAudioHandler());
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  extensions::DispatchVolumeChangedEvent(
      audio_handler->GetOutputVolumePercent(),
      audio_handler->IsOutputMuted());
}
