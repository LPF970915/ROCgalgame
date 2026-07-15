#pragma once

#include <string>

enum class AppTextId {
  MenuTitle = 0,
  SettingSystemControls,
  SettingKeyGuide,
  SettingKeyCalibration,
  SettingClearHistory,
  SettingClearCache,
  SettingTxtToUtf8,
  SettingContributorAvatars,
  SettingContactMe,
  SettingVersionUpdate,
  SettingUrlEntry,
  SettingExitApp,
  ExitHint,
  SystemKeySound,
  SystemBrightness,
  SystemAutoSleep,
  SystemSleepTimer,
  SystemLanguage,
  SystemOn,
  SystemOff,
  SystemClear,
  SystemClearCache,
  SystemClearHistory,
  TxtBackgroundColor,
  TxtFontColor,
  TxtFontSize,
  TxtTranscode,
  TxtStartTranscode,
  VersionCurrentVersion,
  VersionCheckAndUpdate,
  VersionPressAToCheck,
  VersionNoNetwork,
  VersionDownloading,
  VersionDownloadedPackage,
  VersionRestartToInstall,
  VersionAlreadyLatest,
  VersionDownloadFailed,
  KeyGuideProfile34xxSp,
  KeyGuideProfile35xxH,
  KeyGuideProfileOtherH700,
  KeyGuideProfileTrimuiBrick,
  KeyGuideActionDpad,
  KeyGuideActionA,
  KeyGuideActionB,
  KeyGuideActionX,
  KeyGuideActionY,
  KeyGuideActionShoulders,
  KeyGuideActionTriggers,
  KeyGuideActionMenu,
  KeyGuideActionMenuCombo,
  KeyGuideActionVolume,
  ContactRewardHint,
  ContributionValue,
};

int ClampSystemLanguageIndex(int value);
int SystemLanguageCount();
const char *SystemLanguageDisplayLabel(int index);
const char *SystemLanguageConfigValue(int index);
int SystemLanguageIndexFromConfigValue(const std::string &value);
std::string NormalizeSystemLanguageConfigValue(const std::string &value);
const char *LocalizedAppText(int language_index, AppTextId text_id);
const char *LocalizedSleepIntervalLabel(int language_index, int interval_index);
std::string LocalizedDownloadSpeedText(int language_index, double bytes_per_sec);
std::string LocalizedBootScanText(int language_index, size_t current, size_t total);
std::string LocalizedBootCoverText(int language_index, size_t current, size_t total);
std::string LocalizedUpdateReplayText(int language_index, float ratio, bool success, const std::string &version);
std::string LocalizeContributionLabel(int language_index, const std::string &label);
