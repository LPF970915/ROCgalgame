[linemode]

@loadplugin module=extrans.dll   cond=KAGConfigEnabled("extransEnabled",true)
@loadplugin module=extNagano.dll cond=KAGConfigEnabled("extNaganoEnabled")

@if exp="!SystemConfig.referAppendVersion"
	@call storage="version.ks"
@else
	[emb escape=false exp="createCallConfigFile('version.ks')"]
@endif
@nowait
@textwrite enabled=false
@if    exp="typeof tf.__origDebugLevel == 'undefined' && System.getArgument('-debug') != 'yes'"
	; マクロ読み込み時はデバッグログを無効に（長いと起動が遅くなるので）
	@eval exp="tf.__origDebugLevel = tkdlNone, tf.__origDebugLevel <-> kag.debugLevel" 
	;@call storage="macro.ks"
	[emb escape=false exp="createCallConfigFile('macro.ks')"]
	@eval exp="tf.__origDebugLevel <-> kag.debugLevel"
@else
	;@call storage="macro.ks"
	[emb escape=false exp="createCallConfigFile('macro.ks')"]
@endif
;@call storage="custom.ks"
[emb escape=false exp="createCallConfigFile('custom.ks')"]
@endnowait
@textwrite enabled=true

*first
[syshook name="first.init"]
[syshook name="first.logo" cond=!SystemConfig.stopSkipOnMessageReceived]
[sysjump from="first" to="title"]
[s]

*reset
[clearallmacro]
;[call storage="macro.ks"]
[emb escape=false exp="createCallConfigFile('macro.ks')"]
[jump target=*first]
