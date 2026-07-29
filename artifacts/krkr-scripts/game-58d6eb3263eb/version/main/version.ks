; バージョン番号を指定
; パッチをあてる場合はこのファイルも更新する
[eval exp='global.software_version = "1.0"']
[eval exp='global.software_version += GetRevisionString(" / rev:%s")']
[return]
