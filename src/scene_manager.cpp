#include "scene_manager.h"

AppScene SceneManager::Current() const { return current_; }
void SceneManager::EnterShelf() { current_ = AppScene::Shelf; }
void SceneManager::EnterSettings() { current_ = AppScene::Settings; }
void SceneManager::EnterCoreTransition() { current_ = AppScene::CoreTransition; }
bool SceneManager::Is(AppScene scene) const { return current_ == scene; }
