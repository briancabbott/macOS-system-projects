BAD_CONFIG_FILE
	"imposible entender el fichero %s\n"
CONFIG_OPEN_ERROR
	"Aviso: imposible abrir el fichero de configuraci�n %s\n"
PARSE_ERROR_IN_CONFIG
	"Error en el fichero de configuraci�n\n"
INCOMPAT
	"opciones %s y %s incompatibles\n"
NO_ALTERNATE
	"Perd�n - el programa no soporta sistemas alternativos\n"
NO_COMPRESS
	"Man autom�ticamente trata de comprimir las p�ginas, pero\n\
en el fichero de configuraci�n no se define COMPRESS.\n"
NO_NAME_FROM_SECTION
	"�Qu� p�gina de manual de la secci�n %s desea?\n"
NO_NAME_NO_SECTION
	"�Qu� p�gina de manual desea?\n"
NO_SUCH_ENTRY_IN_SECTION
	"No hay ninguna p�gina sobre %s en la secci�n %s\n"
NO_SUCH_ENTRY
	"No hay ninguna p�gina sobre %s\n"
PAGER_IS
	"\nusando %s como paginador\n"
SYSTEM_FAILED
	"Error de preparaci�n o visualizaci�n del manual.\n\
El comando %s termin� con el error %d.\n"
VERSION
	"%s, versi�n %s\n\n"
OUT_OF_MEMORY
	"Memoria agotada - imposible obtener %d bytes\n"
ROFF_CMD_FROM_FILE_ERROR
	"Error en el procesamiento *roff del fichero %s\n"
MANROFFSEQ_ERROR
	"Error procesano MANROFFSEQ. Usando opciones por omisi�n.\n"
ROFF_CMD_FROM_COMMANDLINE_ERROR
	"Error en el procesamiento *roff de la l�nea de comandos.\n"
UNRECOGNIZED_LINE
	"L�nea desconocida en el fichero de configuraci�n (ignorada)\n%s\n"
GETVAL_ERROR
	"man-config.c: error interno: no se encuentra la cadena %s\n"
FOUND_MANDIR
	"encontrado el directorio del manual %s\n"
FOUND_MAP
	"encontrada la correspondencia %s --> %s\n"
FOUND_CATDIR
	"el catdir correspondiente es %s\n"
LINE_TOO_LONG
	"L�nea demasiado larga en el fichero de configuraci�n\n"
SECTION
	"\nsecci�n: %s\n"
UNLINKED
	"%s eliminado\n"
GLOBBING
	"expandiendo %s\n"
EXPANSION_FAILED
	"Tentativa [%s] de descomprimir el manual fallida\n"
OPEN_ERROR
	"Imposible abrir la p�gina sobre %s\n"
READ_ERROR
	"Error durante la lectura de la p�gina sobre %s\n"
FOUND_EQN
	"encontrada la directiva eqn(1)\n"
FOUND_GRAP
	"encontrada la directiva grap(1)\n"
FOUND_PIC
	"encontrada la directiva pic(1)\n"
FOUND_TBL
	"encontrada la directiva tbl(1)\n"
FOUND_VGRIND
	"encontrada la directiva vgrind(1)\n"
FOUND_REFER
	"encontrada la directiva refer(1)\n"
ROFF_FROM_COMMAND_LINE
	"procesando directiva en la l�nea de comandos\n"
ROFF_FROM_FILE
	"procesando directiva en el fichero %s\n"
ROFF_FROM_ENV
	"procesando directiva en el entorno\n"
USING_DEFAULT
	"usando la secuencia de preprocesadores por omisi�n\n"
PLEASE_WAIT
	"Dando formato a la p�gina, espere por favor...\n"
CHANGED_MODE
	"cambio del modo %s a %o\n"
CAT_OPEN_ERROR
	"Imposible escribir en %s.\n"
PROPOSED_CATFILE
	"si es necesario se intentar� escribir en %s\n"
IS_NEWER_RESULT
	"resultado de is_newer() = %d\n"
TRYING_SECTION
	"probando en la secci�n %s\n"
SEARCHING
	"\nbuscando en %s\n"
ALREADY_IN_MANPATH
	"pero %s ya est� en la ruta de b�squeda del manual\n"
CANNOT_STAT
	"�Aviso: no vale el fichero %s!\n"
IS_NO_DIR
	"�Aviso: %s no es un directorio!\n"
ADDING_TO_MANPATH
	"a�adiendo %s a la ruta de b�squeda del manual\n"
PATH_DIR
	"\ndirectorio %s de la ruta"
IS_IN_CONFIG
	"est� en el fichero de configuraci�n\n"
IS_NOT_IN_CONFIG
	"no est� en el fichero de configuraci�n\n"
MAN_NEARBY
	"pero existe un directorio del manual cercano\n"
NO_MAN_NEARBY
	"y no se encontr� cerca ning�n directorio del manual\n"
ADDING_MANDIRS
	"\na�adiendo los directorios de man obligatorios\n\n"
CATNAME_IS
	"cat_name de convert_to_cat () vale: %s\n"
NO_EXEC
	"\nomitiendo el comando:\n  %s\n"
USAGE1
	"uso: %s [-adfhktwW] [secci�n] [-M ruta] [-P paginador] [-S lista]\n\t"
USAGE2
	"[-m sistema] "
USAGE3
	"[-p cadena] nombre ...\n\n"
USAGE4
	"  a : buscar todas las entradas coincidentes\n\
  c : no usar las p�ginas preprocesadas\n\
  d : mostrar informaci�n adicional para depuraci�n de fallos\n\
  D : igual que -d, pero mostrando tambi�n las p�ginas\n\
  f : iqual que whatis(1)\n\
  h : mostrar estos mensajes de ayuda\n\
  k : igual que apropos(1)\n\
  K : buscar una cadena en todas las p�ginas del manual\n"
USAGE5
	"  t : usar troff para preparar las p�ginas solicitadas\n"
USAGE6
        "\
  w : mostrar la ubicaci�n de las p�ginas solicitadas\n\
    (sin argumento: mostar todos los directorios utilizados)\n\
  W : igual que -w, pero mostrando s�lo nombres de ficheros\n\n\
  C fichero   : usar fichero de configuraci�n alternativo\n\
  M ruta      : establecer la ruta de busqueda de p�ginas\n\
  P paginador : usar paginador para ver las p�ginas\n\
  S lista     : lista de secciones (separadas por dos puntos)\n"
USAGE7
	"  m sistema   : buscar manuales para el sistema indicado\n"
USAGE8
	"  p cadena    : preprocesamiento a efectuar\n\
               e - [n]eqn(1)   p - pic(1)    t - tbl(1)\n\
               g - grap(1)     r - refer(1)  v - vgrind(1)\n"
USER_CANNOT_OPEN_CAT
	"y el usuario real tampoco puede abrir el fichero preprocesado\n"
USER_CAN_OPEN_CAT
	"pero el usuario real s� puede abrir el fichero preprocesado\n"
CANNOT_FORK
	"error al lanzar el comando _%s_\n"
WAIT_FAILED
	"error durante la espera del proceso hijo _%s_\n"
GOT_WRONG_PID
	"extra�o... pid incorrecto mientras esperaba un proceso hijo\n"
CHILD_TERMINATED_ABNORMALLY
	"error fatal: _%s_ termin� anormalmente\n"
IDENTICAL
	"La p�gina de manual sobre %s es id�ntica a la de %s\n"
MAN_FOUND
	"Encontrada la(s) p�gina(s):\n"
NO_TROFF
	"error: no se especifica ning�n comando TROFF en %s\n"
NO_CAT_FOR_NONSTD_LL
	"eliminada p�gina preformateada con l�neas de dimensi�n no est�ndar\n"
