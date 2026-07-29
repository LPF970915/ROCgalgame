*start|プロローグ
[initscene]
[quickmenu fadein notify]

;開始シナリオを下記に記入
[ゲーム開始 storage=sc_pr.txt]

;---------------------------------------------------------------------
; 起動処理用スクリプト。コンバートモードかどうかで挙動を変更
*jump|
[jump target=*envstart cond=world_object.playerExecMode ignorewarn]
[next storage=&tf.start_storage target=&tf.start_target]

*envstart|
	[initscene]
	[syscurrent name="game"]
	[scenestart storage=&tf.start_storage target=&tf.start_target]
*envplay|
	[sceneplay]
	[exit storage="start.ks" target="*gameend_title"]
	[s]

*gameend_title
	[call target=*reset]
	[sysjump from="game" to="title"]
	[gotostart]
	[s]

*gameend_logo
	[call target=*reset]
	[sysjump from="game" to="logo"]
	[gotostart]
	[s]

*reset
	[envclear]
	[endrecollection]
	[initbase]
	[cancelskip]
	[cancelautomode]
	[linemode]
	[return]

