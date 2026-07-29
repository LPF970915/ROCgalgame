*
	; 追加トランジション
	@loadplugin module=extNagano.dll

	; 選択肢配置領域の指定
	[selopt uistorage="select" fadetime=150 left=0 top=60 width=1280 height=360 size=32 color=0xFFFFFF edge edgeColor=0 edgeEmphasis=1024 edgeExtent=2]

	; ヒストリレイヤの uipsd
	[historyopt storage="バックログ"]

	; ゲーム中の右クリックメニューのデフォルト設定を変更
	; [sysrclickopt enabled=true call=true storage=sysmenu.ks target=""]

	; メッセージウィンドウの uipsd
	; メッセージウィンドウのオプション
	[meswinopt storage="textwindow"  transparent=true nameAlign=0 nameVAlign=0 layer=message0 visible=false nameAbsolute=201 textAbsolute=202]

	[addSysScript name="game" storage="start"]

	[addSysHook   name="first.logo"  call storage="custom.ks" target=*logo]

	[addSysHook   name="title.loop"  jump storage="custom.ks" target=*title]
	[addSysHook   name="title.extra" jump storage="custom.ks" target=*extra]
	[addSysHook   name="title.game"  call storage="custom.ks" target=*title_game]

	[addSysScript name="title.from.load"   storage="custom.ks" target="*title_restore"]
	[addSysScript name="title.from.option" storage="custom.ks" target="*title_restore"]

	[addSysScript name="title.from.cgmode"       storage="custom.ks" target=*title_restore_cg]
	[addSysScript name="title.from.scenemode"    storage="custom.ks" target=*title_restore_sc]
	[addSysScript name="title.from.soundmode"    storage="custom.ks" target=*title_restore_mu]

	[call target=*macro]
	[syscover visible color=0xFFFFFF]
	[return]

*title_bgm
	[resetbgmvol]
	[title_bgm]
	[return]

*logo
	[title_bgm paused]
	[syscover visible color=0xFFFFFF]

	;ロゴ表示
	[stoptrans]
	[clearlayers page=fore]
	[clearlayers page=back]
	[ev file=brandlogo notrans]
	[sysupdate]

	[clickskip enabled=true]

	[beginskip]
	[syscover time=500 visible=false]
	;[sysvoice name="sys_logo"]
	[se file="sys_logo"]
	[wait time=2000]
	;[sysvoice wait]
	[se wait]
	[endskip]
	[syscover visible=false]

	[beginskip]
	[begintrans]
	[ev hide]
	[endtrans trans=crossfade time=500 sync]
	[wait time=500]
	[endskip]

	[sysupdate]
	[return]


*title_game
	[sysvoice name="game" cond="tf.gamestart==''"]
	[fadeoutbgm time=1000]
	[begintrans]
		[clearlayers page=back]
		[allimage hide delete]
	[endtrans fade=1000 sync]
	[sysvoice wait cond="tf.gamestart==''"]
	[return]

*title
	[quickmenu init]
	[title_show]
	[sysvoice name="title"]
	[jump target=*title_wait]

*title_restore
	[title_show restore]
	[jump target=*title_wait]

*title_extra
	[title_show extra]
	[jump target=*title_wait]

*extra
	[jump                  target=*moviemode cond=tf.lastextra=="mv"]
	[jump storage=title.ks target=*soundmode cond=tf.lastextra=="mu"]
	[jump                  target=*scenemode cond=tf.lastextra=="sc"]
	[jump storage=title.ks target=*cgmode]
;	[title_update group=extra]
;	[jump target=*title_wait]

*title_restore_cg
		[eval exp='tf.lastextra="cg"'][jump target=*title_restore_from_extra]
*title_restore_sc
		[eval exp='tf.lastextra=tf.moviemode?"mv":"sc"'][jump target=*title_restore_from_extra]
*title_restore_mu
		[eval exp='tf.lastextra="mu"'][jump target=*title_restore_from_extra]
*title_restore_from_extra
		[eval exp="delete tf.moviemode"]
		[jump target=*title_restore]

*scenemode
	[eval exp="tf.moviemode=false"]
	[sysjump from=title to=scenemode]
*replaystart
	[initscene]
	[quickmenu fadein notify]
	[jump storage=start.ks target=*jump ignorewarn]
*moviemode
	[eval exp="tf.moviemode=true"]
	[sysjump from=title to=scenemode]
*donemovie
	[linemode]
	[cancelskip]
	[cancelautomode]
	[begintrans]
	[clearlayers page=back]
	[allimage hide delete]
	[syspage uiload page=back]
	[endtrans fade=300 sync]
	[jump storage="scenemode.ks" target=*page_done]


*extra_back_rclick
*extra_back
	[sysse name=title.back]
	[title_update group=main]
	[jump target=*title_wait]

*title_wait
	[rclick enabled=false]
	[rclick enabled jump storage=custom.ks target=*extra_back_rclick cond='Current.propget("currentGroup")=="extra"']
	[jump storage=title.ks target=*wait]

*continue
	[locklink]
	[suspendload]
	[s]
	[gotostart]

*to_quickload
	[history enabled=false]
	[dialog name=load load askload page=0]
	[syshook name="load.start"]
	[jump storage="load.ks" target="*open"] 

*name_input
	[locklink]
	[title_bgm]
	[panel class="CustomNameInputDialog" storage="nameinput"]
	[jump target=*name_cancel cond=tf.nameInputCanceled]
	[begintrans]
		[syspage free page=back]
		[allimage hide delete]
	[endtrans notrans sync]
	[sysvoice name="game"]
	[fadeoutbgm time=1000]
	[panel action=waitFadePanel time=1000]
	[sysvoice wait]
	[dialog done]
	[jump storage="title.ks" target=*game]
*name_cancel
	[donepanel]
	[syspage current page=fore]
	[unlocklink]
	[jump storage="title.ks" target=*wait]

;---------------------------------------------------------------
*macro
	[macro name=sysvoice][endmacro]
	[macro name=title_bgm]
		@eval exp='tf.titlebgm = typeof global.GetTitleBGM == "Object" ? GetTitleBGM() : SystemConfig.TitleBGM'
		@if exp='tf.titlebgm!=""'
			[if exp=%paused|false]
				@playbgm storage=&tf.titlebgm paused
			[elsif exp=%stop|false]
				@fadeoutbgm time=%time
				@bgm stop=%time
			[elsif exp=%play|true]
				@updatebgm * start=start storage=&tf.titlebgm sflag loop
			[endif]
		@endif
		@eval exp='delete tf.titlebgm'
		[endmacro]

	[macro name="title_show"]
		[title_bgm]
		[stoptrans]
		[rclick enabled=false]
		[clickskip enabled=true]
		[dialog name=title]
		[dialog action=setCurrentGroup group=extra cond=mp.extra]

		[begintrans]
			[allimage hide delete]
			[clearlayers page=back]
;;			[ev file=&GetTitleBgFile()]
			[syspage free page=back layer=message1]
			[syspage uiload page=back hide=&!mp.restore]
			[syspage current page=back]
		[endtrans method=%trans|crossfade time=%time|500 rule=%rule vague=%vague sync]
		[syspage current page=fore]
		[endmacro]

	[macro name="title_update"]
		[dialog action=setCurrentGroup group=%group cond='mp.group!=""']
		[begintrans]
			[syspage uiload page=back]
			[syspage current page=back]
		[endtrans method=%trans|crossfade time=%time|300 rule=%rule vague=%vague sync]
		[syspage current page=fore]
		[endmacro]


[return]
