#pragma once

enum class AppScene {
  Shelf,
  Settings,
  CoreTransition,
};

class SceneManager {
public:
  AppScene Current() const;
  void EnterShelf();
  void EnterSettings();
  void EnterCoreTransition();
  bool Is(AppScene scene) const;

private:
  AppScene current_ = AppScene::Shelf;
};
