[linemode]
[emb escape=false exp='Debug.message("FIRST_PROBE begin")']

@loadplugin module=extrans.dll   cond=KAGConfigEnabled("extransEnabled",true)
@loadplugin module=extNagano.dll cond=KAGConfigEnabled("extNaganoEnabled")
[emb escape=false exp='Debug.message("FIRST_PROBE plugins_done")']

@if exp="!SystemConfig.referAppendVersion"
    [emb escape=false exp='Debug.message("FIRST_PROBE version_before")']
    @call storage="version.ks"
    [emb escape=false exp='Debug.message("FIRST_PROBE version_after")']
@else
    [emb escape=false exp='Debug.message("FIRST_PROBE version_config_before")']
    [emb escape=false exp="createCallConfigFile('version.ks')"]
    [emb escape=false exp='Debug.message("FIRST_PROBE version_config_after")']
@endif

@nowait
@textwrite enabled=false
[emb escape=false exp='Debug.message("FIRST_PROBE macro_before")']
@if exp="typeof tf.__origDebugLevel == 'undefined' && System.getArgument('-debug') != 'yes'"
    @eval exp="tf.__origDebugLevel = tkdlNone, tf.__origDebugLevel <-> kag.debugLevel"
    [emb escape=false exp="createCallConfigFile('macro.ks')"]
    @eval exp="tf.__origDebugLevel <-> kag.debugLevel"
@else
    [emb escape=false exp="createCallConfigFile('macro.ks')"]
@endif
[emb escape=false exp='Debug.message("FIRST_PROBE macro_after")']
[emb escape=false exp='Debug.message("FIRST_PROBE custom_before")']
[emb escape=false exp="createCallConfigFile('custom.ks')"]
[emb escape=false exp='Debug.message("FIRST_PROBE custom_after")']
@endnowait
@textwrite enabled=true

*first
[emb escape=false exp='Debug.message("FIRST_PROBE hook_init_before")']
[syshook name="first.init"]
[emb escape=false exp='Debug.message("FIRST_PROBE hook_init_after")']
[emb escape=false exp='Debug.message("FIRST_PROBE hook_logo_before")']
[syshook name="first.logo" cond=!SystemConfig.stopSkipOnMessageReceived]
[emb escape=false exp='Debug.message("FIRST_PROBE hook_logo_after")']
[emb escape=false exp='Debug.message("FIRST_PROBE sysjump_before")']
[sysjump from="first" to="title"]
[emb escape=false exp='Debug.message("FIRST_PROBE sysjump_after")']
[s]

*reset
[clearallmacro]
[emb escape=false exp="createCallConfigFile('macro.ks')"]
[jump target=*first]
