BAD_CONFIG_FILE
	"nie mo�na zrozumie� zawarto�ci pliku %s\n"
CONFIG_OPEN_ERROR
	"Ostrze�enie: nie mo�na otworzy� pliku konfiguracyjnego %s\n"
PARSE_ERROR_IN_CONFIG
	"B��d podczas interpretacji pliku konfiguracyjnego\n"
INCOMPAT
	"niepasuj�ce wzajemnie opcje %s i %s\n"
NO_ALTERNATE
	"Niestety - brak wkompilowanej obs�ugi alternatywnych system�w\n"
NO_COMPRESS
	"Man zosta� skompilowany z automatyczn� kompresj� stron cat,\n\
ale plik konfiguracyjny nie definiuje COMPRESS.\n"
NO_NAME_FROM_SECTION
	"Kt�r� stron� podr�cznika z sekcji %s chcesz przeczyta�?\n"
NO_NAME_NO_SECTION
	"Kt�r� stron� podr�cznika chcesz przeczyta�?\n"
NO_SUCH_ENTRY_IN_SECTION
	"Nie ma strony %s w sekcji %s podr�cznika\n"
NO_SUCH_ENTRY
	"Nie ma strony podr�cznika dla %s\n"
PAGER_IS
	"\nu�ywanie %s jako pagera\n"
SYSTEM_FAILED
	"B��d podczas wykonywania polecenia formatowania lub wy�wietlania.\n\
Polecenie systemowe %s zwr�ci�o status %d.\n"
VERSION
	"%s, wersja %s\n\n"
OUT_OF_MEMORY
	"Brak pami�ci - nie mo�na przydzieli� %d bajt�w pami�ci\n"
ROFF_CMD_FROM_FILE_ERROR
	"B��d podczas przetwarzania polecenia *roff z pliku %s\n"
MANROFFSEQ_ERROR
	"B��d podczas przetwarzania MANROFFSEQ. U�ywanie ustawie� domy�lnych systemu.\n"
ROFF_CMD_FROM_COMMANDLINE_ERROR
	"B��d podczas przetwarzania polecenia *roff podanego w linii polece�.\n"
UNRECOGNIZED_LINE
	"Nierozpoznana linia w pliku konfiguracyjnym (zignorowana)\n%s\n"
GETVAL_ERROR
	"man-config.c: b��d wewn�trzny: nie znaleziono �a�cucha znak�w %s\n"
FOUND_MANDIR
	"znaleziono katalog man %s\n"
FOUND_MAP
	"znaleziono map� manpath %s --> %s\n"
FOUND_CATDIR
	"odpowiadaj�cy katalog cat to %s\n"
LINE_TOO_LONG
	"Za d�uga linia w pliku konfiguracyjnym\n"
SECTION
	"\nsekcja: %s\n"
UNLINKED
	"skasowany %s\n"
GLOBBING
	"globbing %s\n"
EXPANSION_FAILED
	"Pr�ba [%s] rozwini�cia strony podr�cznika nie powiod�a si�\n"
OPEN_ERROR
	"Nie mo�na otworzy� strony podr�cznika %s\n"
READ_ERROR
	"B��d podczas odczytu strony podr�cznika %s\n"
FOUND_EQN
	"znaleziono dyrektyw� eqn(1)\n"
FOUND_GRAP
	"znaleziono dyrektyw� grap(1)\n"
FOUND_PIC
	"znaleziono dyrektyw� pic(1)\n"
FOUND_TBL
	"znaleziono dyrektyw� tbl(1)\n"
FOUND_VGRIND
	"znaleziono dyrektyw� vgrind(1)\n"
FOUND_REFER
	"znaleziono dyrektyw� refer(1)\n"
ROFF_FROM_COMMAND_LINE
	"przetwarzanie dyrektywy podanej w linii polece�\n"
ROFF_FROM_FILE
	"przetwarzanie dyrektywy z pliku %s\n"
ROFF_FROM_ENV
	"przetwarzanie dyrektywy ze zmiennej �rodowiskowej\n"
USING_DEFAULT
	"u�ywanie domy�lnej sekwencji preprocesora\n"
PLEASE_WAIT
	"Formatowanie strony, prosz� czeka�...\n"
CHANGED_MODE
	"zmieniono uprawnienia %s na %o\n"
CAT_OPEN_ERROR
	"Nie mo�na otworzy� %s do zapisu.\n"
PROPOSED_CATFILE
	"w razie potrzeby man b�dzie pr�bowa� zapisa� %s\n"
IS_NEWER_RESULT
	"status z is_newer() = %d\n"
TRYING_SECTION
	"pr�bowanie sekcji %s\n"
SEARCHING
	"\nwyszukiwanie w %s\n"
ALREADY_IN_MANPATH
	"ale %s jest ju� w manpath\n"
CANNOT_STAT
	"Ostrze�enie: nie mo�na u�y� stat na pliku %s!\n"
IS_NO_DIR
	"Ostrze�enie: %s nie jest katalogiem!\n"
ADDING_TO_MANPATH
	"dodawanie %s do manpath\n"
PATH_DIR
	"\n�cie�ka katalogu %s "
IS_IN_CONFIG
	"jest w pliku konfiguracyjnym\n"
IS_NOT_IN_CONFIG
	"nie jest obecna w pliku konfiguracyjnym\n"
MAN_NEARBY
	"ale jest katalog man niedaleko\n"
NO_MAN_NEARBY
	"i nie ma katalogu man niedaleko\n"
ADDING_MANDIRS
	"\ndodawanie obowi�zkowych katalog�w man\n\n"
CATNAME_IS
	"cat_name w convert_to_cat () jest: %s\n"
NO_EXEC
	"\nniewykonywanie polecenia:\n  %s\n"
USAGE1
	"u�ycie: %s [-adfhktwW] [sekcja] [-M �cie�ka] [-P pager] [-S lista]\n\t"
USAGE2
	"[-m system] "
USAGE3
	"[-p �a�cuch_znak�w] nazwa ...\n\n"
USAGE4
	"  a : znajduje wszystkie pasuj�ce strony\n\
  c : nie u�ywa pliku cat\n\
  d : wy�wietla mn�stwo informacji debugowania\n\
  D : jak -d, ale wy�wietla te� strony\n\
  f : to samo co whatis(1)\n\
  h : wy�wietla ten komunikat pomocy\n\
  k : to samo co apropos(1)\n\
  K : wyszukuje �a�cuch znak�w na wszystkich stronach\n"
USAGE5
	"  t : u�ywa troff do formatowania stron do wydrukowania\n"
USAGE6
	"\
  w : wy�wietla po�o�enie stron(y) podr�cznika, kt�ra by�aby wy�wietlona\n\
      (je�eli nie podano �adnej nazwy: wy�wietla przeszukiwane katalogi)\n\
  W : tak jak -w, ale wy�wietla tylko nazwy plik�w\n\n\
  C plik   : u�ywa `plik' jako plik konfiguracyjny\n\
  M �cie�ka: ustawia �cie�k� wyszukiwania stron podr�cznika jako `�cie�ka'\n\
  P pager  : u�ywa programu `pager' do wy�wietlania stron\n\
  S lista  : lista sekcji oddzielona dwukropkami\n"
USAGE7
	"  m system : wyszukuje strony podr�cznika dla alternatywnego systemu\n"
USAGE8
	"  p �a�cuch znak�w: �a�cuch znak�w okre�laj�cy, kt�re preprocesory u�y�\n\
               e - [n]eqn(1)   p - pic(1)    t - tbl(1)\n\
               g - grap(1)     r - refer(1)  v - vgrind(1)\n"
USER_CANNOT_OPEN_CAT
	"a rzeczywisty u�ytkownik te� nie mo�e otworzy� pliku\n"
USER_CAN_OPEN_CAT
	"ale rzeczywisty u�ytkownik mo�e otworzy� plik\n"
CANNOT_FORK
	"nieudana pr�ba fork polecenia _%s_\n"
WAIT_FAILED
	"b��d podczas oczekiwania na proces potomny _%s_\n"
GOT_WRONG_PID
	"bardzo dziwne..., otrzymano z�y pid podczas oczekiwania na proces potomny\n"
CHILD_TERMINATED_ABNORMALLY
	"b��d krytyczny: polecenie _%s_ zako�czone nieprawid�owo\n"
IDENTICAL
	"Strona podr�cznika %s jest identyczna jak %s\n"
MAN_FOUND
	"Znaleziono stron�(y) podr�cznika:\n"
NO_TROFF
	"b��d: nie podano polecenia TROFF w %s\n"
NO_CAT_FOR_NONSTD_LL
	"strona cat nie zosta�a zapisana z powodu niestandardowej d�ugo�ci linii\n"
BROWSER_IS
	"\nu�ywanie %s jako przegl�darki\n"
HTMLPAGER_IS
	"\nu�ywanie %s do zrzucania stron HTML jako tekst"
FOUND_FILE
	"manfile_from_sec_and_dir() znalaz�a %s\n"
CALLTRACE1
	"manfile_from_sec_and_dir(dir=%s, sec=%s, name=%s, flags=0x%0x)\n"
CALLTRACE2
	"glob_for_file(dir=%s, sec=%s, name=%s, type=0x%0x, ...)\n"
NO_MATCH
	"glob_for_file nie znalaz�a �adnych dopasowa�.\n"
GLOB_FOR_FILE
	"glob_for_file zwr�ci�a %s.\n"
CALLTRACE3
	"glob_for_file_ext_glob(dir=%s, sec=%s, name=%s, ext=%s, hpx=%s, glob=%d, type=0x%0x);\n"
ABOUT_TO_GLOB
	"glob_for_file_ext_glob rozwinie %s\n"
