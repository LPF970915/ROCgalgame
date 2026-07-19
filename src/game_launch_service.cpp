#include "game_launch_service.h"

CoreSpecResult GameLaunchService::BuildSpec(const AppConfig &config,
                                            const GameEntry &game) const {
  const IGameCoreAdapter *adapter = registry_.Find(game.core);
  if (!adapter) {
    return CoreSpecResult{LaunchStatus::Unsupported, {}, "core adapter unavailable"};
  }
  return adapter->BuildSpec(config, game);
}

LaunchResult GameLaunchService::Launch(const AppConfig &config,
                                       const GameEntry &game) const {
  CoreSpecResult built = BuildSpec(config, game);
  if (!built.Ok()) {
    LaunchResult result;
    result.status = built.status;
    result.detail = std::move(built.detail);
    result.log_path = built.spec.log_path;
    return result;
  }
  return runner_.Run(built.spec);
}

std::string DescribeLaunchResult(const LaunchResult &result, int language_index) {
  const bool traditional = language_index == 1;
  const bool chinese = language_index == 0 || traditional;
  switch (result.status) {
    case LaunchStatus::NormalExit: return {};
    case LaunchStatus::MissingCore:
      return chinese ? (traditional ? u8"核心檔案不存在" : u8"核心文件不存在")
                     : "Core executable not found";
    case LaunchStatus::InvalidGame:
      return chinese ? (traditional ? u8"遊戲入口無效，請檢查 game.ini" : u8"游戏入口无效，请检查 game.ini")
                     : "Invalid game entry; check game.ini";
    case LaunchStatus::ExecFailure:
      return chinese ? (traditional ? u8"核心啟動失敗" : u8"核心启动失败")
                     : "Failed to start core";
    case LaunchStatus::CoreError:
      return (chinese ? (traditional ? u8"核心異常結束，代碼 " : u8"核心异常退出，代码 ")
                      : "Core exited with code ") + std::to_string(result.exit_code);
    case LaunchStatus::Signaled:
      return (chinese ? (traditional ? u8"核心被訊號終止，訊號 " : u8"核心被信号终止，信号 ")
                      : "Core terminated by signal ") + std::to_string(result.signal);
    case LaunchStatus::Unsupported:
      return chinese ? (traditional ? u8"目前平台不支援該核心" : u8"当前平台不支持该核心")
                     : "Core is not supported on this platform";
  }
  return {};
}
