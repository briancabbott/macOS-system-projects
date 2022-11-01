# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/texts/ja/message_proc.tcl,v 1.2 1998/04/05 15:30:33 robin Exp $
#
# These procedures generate local messages with arguments

proc make_message_phase4 { saveto } {
    global messages
    set messages(phase4.2) \
	    "�ե����� $saveto �ΥХå����åץե����롢$saveto.bak ��\n\
	    ���ʤ��Τǡ��������¸����ޤ���Ǥ�����\n\
	    �ե�����̾���ѹ����ơ���¸��ľ���Ʋ�������"
    set messages(phase4.3) "���ޤǤ�����ե������ $saveto.bak �� \n\
	    �Хå����åפȤ�����¸����ޤ�����"
    set messages(phase4.4) \
	    "�ե����� $saveto ���������¸���뤳�Ȥ��Ǥ��ޤ���\n\n\
	    �ե�����̾���ѹ�������¸��ľ���Ʋ�����"
    set messages(phase4.5) "X �����꤬��λ���ޤ�����\n\n"
}
proc make_message_card { args } {
    global pc98 messages Xwinhome
    global cardServer
    
    set mes ""
    if !$pc98 {
	if ![file exists $Xwinhome/bin/XF86_$cardServer] {
	    if ![string compare $args cardselected] {
		set mes \
			"!!! ���Υ���ե��å������ɤ�ɬ�פʥ����С���\
			���󥹥ȡ��뤵��Ƥ��ޤ�����������Ǥ��ơ�\
			$cardServer �����С��� $Xwinhome/bin/XF86_$cardServer \
			��̾���ǥ��󥹥ȡ��뤷���⤦��������� \
			���ľ���Ʋ������� !!!"
	    } else {
		set mes \
			"!!! ���򤵤줿�����С��ϥ��󥹥ȡ��� \
			����Ƥ��ޤ�����������Ǥ��ơ�\
			$cardServer �����С��� $Xwinhome/bin/XF86_$cardServer \
			��̾���ǥ��󥹥ȡ��뤷���⤦��������� \
			���ľ���Ʋ������� !!!"
	    }
	    bell
	}
    } else {
	if ![file exists $Xwinhome/bin/XF98_$cardServer] {
	    if ![string compare $args cardselected] {
		set mes \
			"!!! ���Υ���ե��å������ɤ�ɬ�פʥ����С���\
			���󥹥ȡ��뤵��Ƥ��ޤ�����������Ǥ��ơ�\
			$cardServer �����С��� $Xwinhome/bin/XF98_$cardServer \
			��̾���ǥ��󥹥ȡ��뤷���⤦��������� \
			���ľ���Ʋ������� !!!"
	    } else {
		set mes \
			"!!! ���򤵤줿�����С��ϥ��󥹥ȡ��� \
			����Ƥ��ޤ�����������Ǥ��ơ�\
			$cardServer �����С��� $Xwinhome/bin/XF98_$cardServer \
			��̾���ǥ��󥹥ȡ��뤷���⤦��������� \
			���ľ���Ʋ������� !!!"	
	    }
	    bell
	}
    }
    return $mes
}

proc make_intro_headline { win } {
    global pc98
    $win tag configure heading \
	    -font -jis-fixed-medium-r-normal--24-230-*-*-c-*-jisx0208.1983-0
    if !$pc98 {
	$win insert end \
		"�أƣ����ӣ������ˤĤ���" heading
    } else {
	$win insert end \
		"�أƣ����ӣ������ˤĤ���" heading
    }
}

proc make_underline { win } {
	$win.menu.mouse configure -underline 4
	$win.menu.keyboard configure -underline 6
	$win.menu.card configure -underline 4
	$win.menu.monitor configure -underline 7
	$win.menu.modeselect configure -underline 4
	$win.menu.other configure -underline 4
	$win.buttons.abort configure -underline 3
	$win.buttons.done configure -underline 5
	$win.buttons.help configure -underline 4
}
