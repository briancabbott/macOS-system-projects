Prvo berilo za Emacs. Pogoji uporabe in raz�irjanja so navedeni na koncu.

Ukazi v Emacsu v splo�nem vklju�ujejo tipki CONTROL (v�asih ozna�eni
CTRL ali CTL) in META (v�asih ozna�ena EDIT ali ALT). Namesto, da bi ju
vedno izpisali s celim imenom, bomo uporabili naslednji okraj�avi:

 C-<znak> pomeni, da moramo dr�ati pritisnjeno tipko CONTROL, ko
 	  vtipkamo <znak>. Oznaka C-f tako pomeni: dr�imo pritisnjeno
 	  tipko CONTROL in pritisnemo tipko f.
 M-<znak> pomeni, da moramo dr�ati pritisnjeno tipko META, EDIT ali
 	  ALT, ko vtipkamo <znak>. �e na tipkovnici ni tipk META, EDIT
 	  ali ALT, pritisnemo tipko ESC, jo spustimo in zatem
 	  pritisnemo tipko <chr>. Tipko ESC bomo ozna�evali z <ESC>.

Pomembno: Emacs zapustimo z ukazom C-x C-c (dva znaka).
V u�beniku so vaje, s katerimi preskusite nove ukaze. Ozna�ujeta jih
znaka ,>>` ob levem robu. Zgled:
<<Blank lines inserted here by startup of help-with-tutorial>>
[Sredina strani je iz didakti�nih razlogov prazna. Besedilo se nadaljuje spodaj]
>> Vtipkajte zdaj ukaz C-v (View next screen, Prika�i naslednji zaslon),
	da se premaknete na naslednji zaslon (kar poskusite, pritisnite
	hkrati tipko CONTROL in V). Od zdaj naprej boste morali to
	napraviti sami vsaki�, ko pridete do konca zaslona.

Ste opazili, da sta se dve vrstici s prej�njega zaslona ponovili? Ta
kontinuiteta olaj�a branje pri skakanju s strani na stran.

Prva stvar, ki si jo morate zapomniti, je, kako se premikate po
datoteki. Zdaj �e veste, da se premaknete za cel zaslon naprej z
ukazom C-v. Za cel zaslon nazaj pa se premaknete z ukazom M-v
(pritisnite tipko META in jo dr�ite ter pritisnite tipko v, ali pa
pritisnite in spustite <ESC> ter zatem pritisnite tipko v, �e tipke
META, EDIT ali ALT na va�i tipkovnici ni).

>>  Nekajkrat pritisnite M-v in C-v, da vidite, kako ukaza delujeta.


* POVZETEK
----------

Za pregled celega zaslona besedila so uporabni naslednji ukazi:

	C-v	Premik se za cel zaslon naprej
	M-v	Premik se za cel zaslon nazaj
	C-l	Cel zaslon premaknemo tako, da je zdaj po vertikali
		 osredninjen okoli besedila, kjer se nahaja kazal�ek
		 (znak v C-l je �rka L, ne �tevka 1)

>> Poi��ite kazal�ek na zaslonu in si zapomnite besedilo okoli njega.
   Vtipkajte C-l.
   Ponovno poi��ite kazal�ek. Besedilo okoli njega je ostalo isto.

Za premikanje za cel zaslon naprej ali nazaj lahko tipkovnicah, ki
imajo ti tipki, uporabljate tudi PageUp in PageDown. Opisan postopek s
C-v in M-v pa deluje povsod.


* PREMIKANJE KAZAL�KA
---------------------

Premiki za celo stran naprej in nazaj so sicer uporabni, ampak kako pa
pridemo do izbranega mesta na zaslonu?

Na�inov je ve�. Najosnovnej�i je uporaba ukazov C-p, C-b, C-f in
C-n. Ti po vrsti premaknejo kazal�ek v prej�njo vrstico, znak nazaj,
znak naprej, in v naslednjo vrstico. Ti �tirje ukazi so enakovredni
kurzorskim tipkam:

			  prej�nja vrstica, C-p
				  :
				  :
       nazaj, C-b .... trenutni polo�aj kazal�ka .... naprej, C-f
				  :
				  :
			  naslednja vrstica, C-n

>> S pritiski na C-n ali C-p premaknite kazal�ek v sredinsko vrstico
   na diagramu zgoraj. Zatem pritisnite C-l. S tem diagram postavite na
   sredino zaslona.

V angle��ini ima izbor tipk nazoren pomen. P kot ,previous`
(prej�nji), N kot ,next` (naslednji), B kot ,backward` (nazaj) in F
kot ,forward` (naprej). Te osnovne ukaze za premikanje kazal�ka boste
uporabljali ves �as.

>> Nekajkrat pritisnite C-n, da pride kazal�ek do te vrstice.

>> Z nekaj C-f se pomaknite na desno na sredo vrstice, nato pa nekajkrat
   pritisnite C-p. Opazujte, kaj se dogaja s kazal�kom na sredini
   vrstice.

Vsaka vrstice v besedilu je zaklju�ena z znakom za novo vrstico
(angl. Newline). Ta lo�uje vrstico v besedilu od naslednje. Tudi
zadnja vrstica v datoteki mora biti zalju�ena z znakom za novo vrstico
(�eprav tega Emacs ne zahteva).

>> Poskusite ukaz C-b, ko je kazal�ek na za�etku vrstice. Kazal�ek se
   mora premakniti na konec prej�nje vrstice. To je zato, ker se je
   ravnokar premaknil prek znaka za konec vrstice.

Ukaz C-f premika kazal�ek prek znaka za novo vrstico enako kot C-b.

>> Poskusite �e nekajkrat pritisniti C-b, da dobite ob�utek za
   premikanje kazal�ka. Potem nekajkrat poskusite C-f, da pridete do konca
   vrstice. �e enkrat pritisnite C-f, da sko�ite v naslednjo vrstico.

Ko s kazal�kom dose�ete zgornji ali spodnji rob zaslona, se besedilo
toliko premakne, da kazal�ek ostane na zaslonu. V angle��ini se temu
pravi ,,scrolling``. To omogo�a, da lahko premaknemo kazal�ek na
katerokoli mesto v besedilu, a vseeno ostanemo na zaslonu.

>> Poskusite kazal�ek pripeljati s C-n �isto do dna zaslona in si oglejte,
   kaj se zgodi.

�e se vam zdi premikanje po en znak prepo�asno, se lahko premikate za
celo besedo. M-f (META-f) premakne kazal�ek za eno besedo naprej, M-b
pa za besedo nazaj.

>> Poskusite nekajkrat M-f in M-b.

�e je kazal�ek sredi besede, ga M-f prestavi na konec besede. �e je v
belini med besedami, ga M-f premakne na konec naslednje besede. M-b
deluje podobno, a v nasprotni smeri.

>> Nekajkrat poskusite M-f in M-b, vmes pa �e nekaj C-f in
   C-b. Opazujte u�inke M-f in M-b, ko je kazal�ek sredi besede ali
   med besedami.

Ste opazili paralelo med C-f in C-b na eni strani ter M-f in M-b na
drugi? V Emacsu se dostikrat ukazi Meta nana�ajo na operacije nad
enotami jezika (besede, stavki, odstavki), medtem ko se ukazi Control
nana�ajo na operacije, neodvisne od zvrsti besedila (znaki, vrstice
ipd.).

Podobna zveza je tudi med vrsticami in stavki: ukaza C-a in C-e
premakneta kazal�ek na za�etek oz. konec vrstice, M-a in M-e pa na
za�etek oz. konec stavka.

>> Poskusite nekaj ukazov C-a, potem pa nekaj ukazov C-e.
   Poskusite nekaj ukazov M-a, potem pa nekaj ukazov M-e.

Ste opazili, da ponovljeni C-a ne napravijo ni�, ponovljeni M-a pa se
premikajo naprej? �eprav se ne obna�ata enako, pa je vendar obna�anje
enega in drugega po svoje naravno.

Polo�aju kazal�ka na zaslonu pravimo tudi ,,point``, to�ka.
Parafrazirano: kazal�ek ka�e na zaslonu, kje je to�ka v besedilu.

Povzetek preprostih ukazov za premikanje kazal�ka, vklju�no s premiki
po besedo in stavek:

	C-f	Premik za znak naprej
	C-b	Premik za znak nazaj

	M-f	Premik za besedo naprej
	M-b	Premik za besedo nazaj

	C-n	Premik v naslednjo vrstico
	C-p	Premik v prej�njo vrstico

	C-a	Premik na za�etek vrstice
	C-e	Premik na konec vrstice

	M-a	Premik na za�etek stavka
	M-e	Premik na konec stavka

>> Za vajo nekajkrat poskusite vsakega od teh ukazov.
   To so najpogosteje uporabljani ukazi.

�e dva pomembna ukaza za premikanje kazal�ka sta M-< (META-manj�i od),
ki ga premakne na za�etek datoteke, in M-> (META-ve�ji od), ki ga
premakne na konec datoteke.

Na ameri�kih tipkovnicah najdete znak < nad vejico in morate
pritisniti tipko Shift, da pridete do njega. Z ukazom M-< je enako -
prav tako morate pritisniti tipko Shift, sicer moste izvedli drug
ukaz, Meta-vejica. Na na�ih tipkovnicah sta oba znaka na isti tipko,
in za ukaz M-> morate pritisniti �e tipko Shift.

>> Poskusite zdaj M-<, skok na za�etek tega u�benika.
   Potem se vrnite nazaj z zaporednimi C-v.

>> Poskusite zdaj M->, skok na konec tega u�benika.
   Potem se vrnite nazaj z zaporednimi M-v.

�e ima va�a tipkovnica kurzorske tipke, lahko premikate kazal�ek po
zaslonu tudi z njimi. Vseeno priporo�amo, da se privadite ukazov C-b,
C-f, C-n in C-p, in to iz treh razlogov. Prvi�, delujejo na �isto vseh
terminalih. Drugi�, z nekaj prakse v Emacsu boste opazili, da je
tipkanje ukazov s CONTROL hitrej�e od tipkanja s kurzorskimi tipkami, ker
ni treba ves �as premikati desnice s tipkovnice na kurzorske tipke in
nazaj. In tretji�, ko se enkrat navadite teh ukazov s CONTROL, se boste
enostavneje nau�ili tudi bolj zapletenih ukazov za premikanje kazal�ka.

Ve�ini ukazov v Emacsu lahko podamo �tevil�ni argument; najve�krat ta
pove, kolikokrat zapovrstjo naj se ukaz izvede. Ve�kratno ponovitev
ukaza izvedemo tako, da najprej vtipkamo C-u, zatem �tevilo,
kolikokrat naj se ukaz ponovi, in nazadnje �eljeni ukaz. �e ima va�a
tipkovnica tipko META (ali EDIT ali ALT), lahko izpustite ukaz C-u in
namesto tega vtipkate �tevilo ponovitev, medtem ko dr�ite pritisnjeno
tipko META. Druga metoda je sicer kraj�a, priporo�amo pa prvo, ker
deluje na vseh terminalih. Tak�en �tevil�ni argument je ,,prefiksni``
argument, ker vnesemo argument pred ukazom, na katerega se nana�a.

Zgled: C-u 8 C-f premakne kazal�ek za osem znakov naprej.

>> Poskusite s primernim argumentom za �tevilo ponovitev ukaza
   C-n ali C-p priti �im bli�e tej vrstici v enem samem skoku.

Ve�ina ukazov, ne pa vsi, uporablja �tevil�ni argument kot �tevilo
ponovitev ukaza. Nekateri ukazi - nobeden od tistih, ki smo si jih
ogledali do zdaj - ga uporabljajo kot stikalo: s podanim prefiksnim
argumentom napravi ukaz nekaj drugega kot obi�ajno.

Ukaza C-v in M-v sta tudi izjemi, a druga�ni. �e jima podamo argument,
premakneta zaslon za navedeno �tevilo vrstic, ne pa zaslonov. Ukaz C-u
8 C-v, na primer, premakne zaslon navzgor za 8 vrstic.

>> Poskusite zdaj C-u 8 C-v

To bi moralo zaslon premakniti navzgor za osem vrstic. �e bi ga radi
premaknili nazaj, poskusite M-v z istim argumentom.

�e uporabljate grafi�ni vmesnik, denimo X11 ali MS Windows, imate
verjetno ob robu Emacsovega okna navpi�no pravokotno ploskev,
imenovano drsnik. Pogled na besedilo lahko premikate tudi tako, da z
mi�ko kliknete na drsnik.

>> Postavite kazalec na vrh ozna�enega obmo�ja na drsniku in pritisnite
   srednji gumb na mi�ki. To bi moralo premakniti besedilo na mesto,
   dolo�eno s tem, kako visoko ali nizko na drsnik ste kliknili.

>> Medtem ko dr�ite srednji gumb pritisnjen, premikajte mi�ko gor in
   dol. Vidite, kako se premika besedilo v Emacsovem oknu, ko
   premikate mi�ko?


* �E SE EMACS OBESI
-------------------

�e se Emacs preneha odzivati na va�e ukaze, ga lahko varno prekinete z
ukazom C-g. Z njim lahko prekinete ukaze, za katere bi trajalo
predolgo, da bi se izvedli.

Isti ukaz, C-g, lahko uporabite tudi, da prekli�ete �tevil�ni
argument, ali pa za�etek ukaza, ki ga ne �elite izvesti.

>> Vtipkajte C-u 100, s �imer ste izbrali �tevil�ni argument 100,
   zatem pa vtipkajte C-g. Vtipkajte zdaj C-f. Kazal�ek se je
   premaknil le za en znak, ker ste �tevil�ni argument vmes preklicali
   s C-g.

Tudi �e ste po nesre�i vtipkali <ESC>, se ga lahko znebite s C-g.


* ONEMOGO�ENI UKAZI
-------------------

Nekaj ukazov v Emacsu je namenoma ,,onemogo�enih``, da bi jih
za�etniki ne izvedli po nesre�i.

�e vtipkate tak onemogo�en ukaz, se bo na zaslonu pojavilo novo okno z
obvestilom, kateri ukaz ste sku�ali izvesti, in vas vpra�alo, �e ga
res �elite izvesti.

�e v resnici �elite poskusiti ukaz, pritisnite preslednico kot odgovor
na vpra�anje. Normalno verjetno ukaza ne �elite izvesti, zato na
vpra�anje odgovorite z ,n`.

>> Vtipkajte C-x C-l (ki je onemogo�en ukaz),
   zatem na vpra�anje odgovorite n.


* OKNA
------

Emacs lahko prika�e ve� oken in v vsakem svoje besedilo. Kasneje bomo
razlo�ili, kako uporabljamo ve� oken hkrati. Zaenkrat bomo povedali
le, kako se znebite dodatnih oken, ki jih lahko odpre vgrajena pomo� ali
pa izpis kak�nega drugega programa. Preprosto je:

	C-x 1   Eno okno (torej, zaprimo vsa ostala).

To je CONTROL-x, ki mu sledi �tevka 1. Ukaz C-x 1 raztegne �ez cel
zaslon okno, v katerem se nahaja kazal�ek, ostala pa zapre.

>> Premaknite kazal�ek do te vrstice in vtipkajte C-u 0 C-l
>> Vtipkajte CONTROL-h k CONTROL-f.
   Vidite, kako se je to okno skr�ilo in odstopilo prostor oknu,
   ki pojasnjuje ukaz CONTROL-f?

>> Vtipkajte C-x 1 in spodnje okno se bo zaprlo.

Za razliko od ukazov, ki smo se jih nau�ili do zdaj, je ta ukaz
sestavljen iz dveh znakov. Za�ne se z znakom CONTROL-x. Cela vrsta
ukazov se za�ne enako, in mnogi od njih zadevajo delo z datotekami,
delovnimi podro�ji in podobnim. Vsem tem ukazom je skupno, da se
za�nejo s CONTROL-x, ki mu sledi �e en, dva ali trije znaki.


* VRIVANJE IN BRISANJE
----------------------

�e �elite v obstoje�e besedilo vriniti novo, preprosto premaknite
kazal�ek na �eljeno mesto in za�nite tipkati. Znake, ki jih lahko
vidite, na primer A, 7, * in podobno, razume Emacs kot del besedila in
jih takoj vrine. S pritiskom na Return (ali Enter) vrinete znak za
skok v novo vrstico.

Zadnji vtipkani znak lahko izbri�ete s pritiskom na tipko
<Delback>. To je tista tipka na tipkovnici, ki jo navadno uporabljate
za brisanje nazadnje natipkanega znaka. Navadno je to velika tipka
vrstico ali dve nad tipko <Return>, ki je ozna�ena z "Backspace",
"Delete" ali "Del".

�e imate na tipkovnici tipko "Backspace", je to tipka <Delback>. Naj
vas ne zmede, �e imate poleg tega �e tipko "Delete" - <Delback> je
"Backspace".

Splo�no <Delback> pobri�e znak neposredno pred trenutnim polo�ajem
kazal�ka.

>> Vtipkajte zdaj nekaj znakov in jih zatem s tipko <Delback> pobri�ite.
   Ni� naj vas ne skrbi, �e se je ta vrstica spremenila. Izvirnika
   tega u�benika ne boste pokvarili -- tole je samo va�a osebna kopija.

Ko vrstica postane predolga za zaslon, se ,,nadaljuje`` v naslednji
vrstici na zaslonu. Obrnjena po�evnica (znak ,\`) ali v grafi�nih
okoljih zavita pu��ica ob desnem robu ozna�uje vrstico, ki se
nadaljuje v naslednji zaslonski vrstici.

>> Zdaj za�nite tipkati besedilo, dokler ne dose�ete desnega roba, in
   �e naprej. Opazili boste, da se pojavi znak za nadaljevanje.

>> S tipko <Delback> pobri�ite toliko znakov, da vrstica ne sega
   ve� �ez �irino zaslona. Znak za nadaljevanje v naslednji
   vrstici je izginil.

Znak za novo vrstico lahko pobri�emo enako kot vsak drug znak. S tem,
ko pobri�emo znak za novo vrstico, zdru�imo vrstici v eno samo.  �e bo
nova vrstica predolga, da bi cela pri�la na zaslon, bo razdeljena v
ve� zaslonskih vrstic.

>> Premaknite kazal�ek na za�etek vrstice in pritisnite <Delback>. To
   zdru�i vrstico s prej�njo.

>> Pritisnite <Return>. S tem ste ponovno vrinili znak za skok v novo
   vrstico, ki ste ga malo prej zbrisali.

Spomnimo se, da lahko za ve�ino ukazov v Emacsu dolo�imo, naj se
izvedejo ve�krat zaporedoma; to vklju�uje tudi vnos teksta. Ponovitev
obi�ajnega znaka ga ve�krat vrine v besedilo.

>> Poskusite zdaj tole: da vnesete osem zvezdic, vtipkajte C-u 8 *

Zdaj ste se nau�ili najpreprostej�i na�in, da v Emacsu nekaj natipkate
in popravite. Bri�ete lahko tudi besede ali vrstice. Tu je povzetek
ukazov za brisanje:

	<Delback>    pobri�e znak tik pred kazal�kom (levo od
	             oznake za kazal�ek)
	C-d   	     pobri�e znak tik za kazal�kom (,pod` oznako
		     za kazal�ek)

	M-<Delback>  pobri�e besedo tik pred kazal�kom
	M-d	     pobri�e besedo tik za kazal�kom

	C-k          zavr�e besedilo desno od kazal�ka do konca vrstice
	M-k          zavr�e besedilo od polo�aja kazal�ka do konca stavka

�rka ,d` je iz angle�ke besede ,delete` (pobrisati), �rka ,k` pa iz
besede ,kill` (pobiti). Ste opazili, da <Delback> in C-d na eni, ter
M-<Delback> in M-d na drugi strani nadaljujeta paralelo, ki sta jo za�ela
C-f in M-f (<Delback> pravzaprav ni kontrolni znak, kar pa naj nas ne
moti).  C-k in M-k sta v enakem sorodu s C-e in M-e: prvi deluje na
vrstice, drugi na stavke.

Obstaja tudi splo�en postopek za brisanje kateregakoli dela delovnega
podro�ja. Kazal�ek postavimo na en konec podro�ja, ki ga �elimo
izbrisati, in pritisnemo C-@ ali C-SPC (SPC je
preslednica). Katerikoli od obeh ukazov deluje. Premaknite kazal�ek na
drug konec podro�ja, ki ga �elite izbrisati, in pritisnite C-w. S tem
ste zavrgli vse besedilo med obema mejama.

>> Premaknite kazal�ek na �rko O, s katero se za�enja prej�nji
   odstavek.
>> Vtipkajte C-SPC. Emacs prika�e sporo�ilo "Mark set" (slov. Oznaka
   postavljena) na dnu ekrana.
>> Premaknite kazal�ek na �rko V v "postavimo" v drugi vrstici istega
   odstavka.
>> Vtipkajte C-w. S tem zavr�emo vse besedilo za�en�i z O in vse do
   �rke V.

Razlika med tem, �e zavr�ete cel odstavek besedila (angl. ,,kill``,
pobiti) ali pa �e pobri�ete znak (angl. ,,delete``), je ta, da lahko
prvega vrnete nazaj z ukazom C-y, drugega pa ne. Na splo�no ukazi, ki
lahko povzro�ijo veliko �kode (pobri�ejo veliko besedila), shranijo
pobrisano besedilo; tisti, ki pobri�ejo samo posamezni znak, ali samo
prazne vrstice in presledke, pa ne.

>> Postavite kazal�ek na za�etek neprazne vrstice. Pritisnite C-k, da
   pobri�ete vsebino vrstice.
>> �e enkrat pritisnite C-k. To pobri�e �e znak za novo vrstico.

Ste opazili, da prvi C-k pobri�e vsebino vrstice, naslednji C-k pa �e
vrstici samo, s �imer se vse besedilo pod biv�o vrstico premakne za
eno vrstico navzgor? Ukaz C-k obravnava �tevil�ni argument malo
druga�e: pobri�e toliko in toliko vrstic z vsebinami vred. To ni zgolj
ponovitev. C-u 2 C-k pobri�e dve polni vrstici besedila, kar je nekaj
drugega, kot �e dvakrat vtipkate C-k.

Besedilo, ki ste ga prej pobili, lahko povrnete (angl.  ,,yank`` --
potegniti). Predstavljajte si, kot da potegnete nazaj nekaj, kar vam
je nekdo odnesel. Pobito besedilo lahko potegnete nazaj na isti ali pa
na kak�en drug kraj v besedilu, ali pa celo v kaki drugi
datoteki. Isto besedilo lahko ve�krat potegnete nazaj, tako da je v
delovnem podro�ju pove�terjeno.

Ukaz za vra�anje pobitega besedila je C-y.

>> Poskusite z ukazom C-y povrniti pobrisano besedilo.

�e ste uporabili ve� zaporednih ukazov C-k, je vse pobrisano besedilo
shranjeno skupaj, in en sam C-y bo vrnil vse tako pobrisane vrstice.

>> Poskusite, nekajkrat vtipkajte C-k.

Zdaj pa vrnimo pobrisano besedilo:

>> Vtipkajte C-y. Zdaj pa premaknite kazal�ek za nekaj vrstic navzdol
   in �e enkrat vtipkajte C-y. Vidite zdaj, kako se kopira dele
   besedila?

Kaj pa, �e ste pobrisali nekaj besedila, ki bi ga radi vrnili, vendar
ste za iskanim odlomkom pobrisali �e nekaj? C-y vrne samo nazadnje
pobrisan odlomek. Vendar tudi prej�nje besedilo ni izgubljeno. Do
njega lahko pridete z ukazom M-y. Ko ste vrnili nazadnje zbrisano
besedilo s C-y, pritisnite M-y, ki ga zamenja s predzanje pobrisanim
besedilom. Vsak naslednji M-y prika�e �e eno prej. Ko ste kon�no
pri�li do iskanega besedila, ni treba napraviti ni� posebnega, da bi
ga obdr�ali. Preprosto nadaljujte z urejanjem, in vrnjeno besedilo bo
ostalo, kamor ste ga odlo�ili.

�e pritisnete M-y dovolj velikokrat, se boste vrnili na za�ete, torej
spet na zadnje pobrisano besedilo.

>> Pobri�ite vrstico, premaknite se nekam drugam, in pobri�ite �e
   eno vrstico.
   Z ukazom C-y dobite nazaj to drugo vrstico.
   Z ukazom M-y pa jo zamenjate s prvo vrstico.
   Ponovite ukaz M-y �e nekajkrat in si oglejte, kaj dobite na
   zaslon. Ponavljajte ga, dokler se ne prika�e ponovno nazadnje
   pobrisana vrstica, in �e naprej. �e �elite, lahko tudi ukazu
   M-y podate pozitivno ali negativno �tevilo ponovitev.


* PREKLIC UKAZA (UNDO)
----------------------

�e ste besedilo spremenili, a ste se kasneje premislili, lahko
besedilo vrnete v prvotno stanje z ukazom Undo, C-x u. Normalno vrne
C-x u zadnjo spremembo besedila; �e ukaz ponovimo, prekli�emo �e
predzadnjo spremembo, in vsaka nadaljnja ponovitev se�e �e eno
spremembo globlje v zgodovino.

Emacs hrani bolj ali manj celotno zgodovino na�ih ukazov, z dvema
izjemama: ukazov, ki niso napravili nobene spremembe v besedilu
(npr. premik kazal�ka), ne shranjuje, in zaporedje do 20 vrinjenih
znakov shrani kot en sam ukaz. Slednje prihrani nekaj ukazov C-x u, ki
bi jih morali vtipkati.

>> Pobri�ite to vrstico z ukazom C-k, potem jo prikli�ite nazaj s C-x u.

C-_ je alternativni ukaz za preklic zadnjega ukaza.  Deluje enako kot
s C-x u, ga je pa la�je odtipkati, �e morate ukaz ponoviti ve�krat
zaporedoma. Te�ava z ukazom C-_ je, da na nekaterih tipkovnicah ni
povsem o�itno, kako ga vtipkati, zato je podvojen �e kot C-x u. Na
nekaterih terminalih moramo na primer vtipkati /, medtem ko dr�imo
pritisnjeno tipko CONTROL.

�e podamo ukazu C-_ ali C-x u numeri�ni argument, je to enako, kot �e
bi ukaz ro�no ponovili tolikokrat, kot pravi argument.

Ukaz za brisanje besedila lahko prekli�ete in besedilo povrnete,
enako, kot �e bi besedilo pobili. Razlika med brisanjem in pobijanjem
besedila je le ta, da le slednje lahko potegnete nazaj z ukazom
C-y. Preklic ukaza pa velja za eno in drugo.


* DATOTEKE
----------

Da bi bile spremembe v besedilu trajne, morate besedilo shraniti v
datoteko. V nasprotnem primeru jih boste za vedno izgubili tisti hip,
ko boste zapustili Emacs. Besedilo postavimo v datoteko tako, da
na disku ,,poi��emo`` (angl. find) datoteko, preden za�nemo tipkati
(pravimo tudi, da ,,obi��emo`` datoteko).

Poiskati datoteko pomeni, da v Emacsu vidimo vsebino datoteke. To je
bolj ali manj tako, kot da z Emacsom urejamo datoteko samo. Vendar pa
spremembe ne postanejo trajne, dokler datoteke ne shranimo
(angl. save) na disk. Tako imamo mo�nost, da se izognemo temu, da bi
nam na pol spremenjene datoteke le�ale po disku, kadar tega ne
�elimo. Ker pa Emacs ohrani izvorno datoteko pod spremenjenim imenom,
lahko prvotno datoteko prikli�emo nazaj celo �e potem, ko smo datoteko
�e shranili na disk.

V predzadnji vrstici na dnu zaslona vidite vrstico, ki se za�ne in
kon�a z vezaji, in vsebuje niz znakov ,,--:-- TUTORIAL``. Ta del
zaslona navadno vsebuje ime datoteke, ki smo jo obiskali. Zdajle je to
,,TUTORIAL``, va�a delovna kopija u�benika Emacsa.  Ko boste poiskali
kak�no drugo datoteko, bo na tem mestu pisalo njeno ime.

Posebnost ukaza za iskanje datoteke je, da moramo povedati, katero
datoteko i��emo. Pravimo, da ukaz ,,prebere argument s terminala`` (v
tem primeru je argument ime datoteke).  Ko vtipkate ukaz

	C-x C-f   (poi��i datoteko)

vas Emacs povpra�a po imenu datoteke. Kar vtipkate, se sproti vidi v
vrstici na dnu zaslona. Temu delovnemu podro�ju pravimo pogovorni
vmesnik (minibuffer), kadar se uporablja za tovrstni vnos. Znotraj
pogovornega vmesnika lahko uporabljate obi�ajne ukaze za urejanje, �e
ste se na primer pri tipkanju zmotili.

Sredi tipkanja imena datoteke (ali katerega koli drugega opravila v
pogovornem vmesniku) lahko ukaz prekli�ete s C-g.

>> Vtipkajte C-x C-f, zatem pa �e C-g. Zadnji ukaz od treh je
   zaprl pogovorni vmesnik in tudi preklical ukaz C-x C-f, ki je
   uporabljal pogovorni vmesnik. Konec z iskanjem datoteke.

Ko ste dokon�ali ime, ga vnesete s pritiskom na <Return>. S tem se
po�ene ukaz C-x C-f in poi��e iskano datoteko. Pogovorni vmesnik
izgine, ko je ukaz izveden.

Trenutek kasneje se vsebina datoteke pojavi na zaslonu. Zdaj lahko
dopolnjujete, urejate ali kako druga�e spreminjate vsebino. Ko �elite,
da ostanejo spremembe trajne, izvedete ukaz:

	C-x C-s   (shrani datoteko)

Besedilo se s tem shrani iz pomnilnika ra�unalnika na datoteko na
disk. Ko prvi� izvedete ta ukaz, se izvorna datoteka preimenuje, tako
da ni izgubljena. Najdete jo pod novim imenom, ki se od starega
razlikuje po tem, da ima na koncu pripet znak ,,~``.

Ko je Emacs shranil datoteko, izpi�e njeno ime. Shranjujte raje
pogosteje kot ne, da v primeru, �e gre z ra�unalnikom kaj narobe, ne
izgubite veliko.

>> Vtipkajte C-x C-s, s �imer boste shranili svojo kopijo tega
   u�benika. Emacs bo v vrstici na dnu zaslona izpisal ,,Wrote
   ...TUTORIAL``.

Opozorilo: na nekaterih sistemih bo ukaz C-x C-s zamrznil zaslon, in
tako ne boste videli, da Emacs �e kaj izpi�e. To je znak, da je
operacijski sistem prestregel znak C-s in ga interpretiral kot znak za
prekinitev toka podatkov, namesto da bi ga posredoval Emacsu. Zaslon
,,odmrznete`` z ukazom C-q. �e je va� sistem eden takih, si za nasvet,
kako re�iti to nev�e�nost, oglejte razdelek ,,Spontaneous Entry to
Incremental Search`` v priro�niku za Emacs.

Poi��ete lahko lahko �e obstoje�o datoteko, da si jo ogledate ali
popravite, ali pa tudi datoteko, ki �e ne obstaja. To je na�in, kako z
Emacsom ustvarimo novo datoteko: poi��ite datoteko z izbranim imenom,
ki bo sprva prazna, in za�nite pisati. Ko jo boste prvi� shranili, bo
Emacs ustvaril datoteko z vne�enim besedilom. Od tod dalje delate na
�e obstoje�i datoteki.


* DELOVNA PODRO�JA
------------------

Tudi �e ste z ukazom C-x C-f poiskali in odprli drugo datoteko, prva
ostane v Emacsu. Nanjo se vrnete tako, da jo �e enkrat ,,poi��ete`` z
ukazom C-x C-f. Tako imate lahko v Emacsu hkrati kar precej datotek.

>> Ustvarite datoteko z imenom ,,bla`` tako, da vtipkate C-x C-f
   bla <Return>. Natipkajte nekaj besedila, ga po potrebi popravite, in
   shranite v datoteko ,,bla`` z ukazom C-x C-s. Ko ste kon�ali, se
   vrnite v u�benik z ukazom C-x C-f TUTORIAL <Return>.

Emacs hrani besedilo vsake datoteke v takoimenovanem ,,delovnem
podro�ju`` (angl. buffer). Ko poi��emo datoteko, Emacs ustvari zanjo
novo delovno podro�je. Vsa obstoje�a delovna podro�ja v Emacsu vidimo
z ukazom:

	C-x C-b   Seznam delovnih podro�ij.

>> Poskusite C-x C-b zdaj.

Vidite, da ima vsako delovno podro�je svoje ime, pri nekaterih pa pi�e
tudi ime datoteke, katere vsebina se hrani v njem. Vsako besedilo, ki
ga vidite v katerem od Emacsovih oken, je vedno del kak�nega delovnega
podro�ja.

>> Z ukazom C-x 1 se znebite seznama delovnih podro�ij.

Tudi �e imate ve� delovnih podro�ij, pa je vedno le eno od njih
trenutno dejavno. To je tisto delovno podro�je, ki ga popravljate. �e
�elite popravljati drugo delovno podro�je, morate ,,preklopiti``
nanj. �e bi radi preklopili na delovno podro�je, ki pripada kak�ni
datoteki, �e poznate en na�in, kako to storiti: ponovno ,,obi��ete``
(odprete) to datoteko z ukazom C-x C-f. Obstaja pa �e la�ji na�in: z
ukazom C-x b. Pri tem ukazu morate navesti ime delovnega podro�ja.

>> Vtipkajte C-x b bla <Return>, s �imer se vrnete v delovno podro�je
   ,,bla`` z vsebino datoteke ,,bla``, ki ste jo maloprej
   odprli. Zatem vtipkajte C-x b TUTORIAL <RETURN>, s �imer se vrnete
   nazaj v ta u�benik.

Ve�inoma se ime delovnega podro�ja kar ujema z imenom datoteke (brez
poti do datoteke), ne pa vedno. Seznam delovnih podro�ij, ki ga
prika�e ukaz C-x C-b, prika�e imena vseh delovnih podro�ij.

Vsako besedilo, ki ga vidite v katerem od Emacsovih oken, je vedno del
kak�nega delovnega podro�ja. Nekatera delovna podro�ja ne pripadajo
nobeni datoteki. Podro�je ,,*Buffer List*``, na primer, je �e eno
takih. To delovno podro�je smo ustvarili ravnokar, ko smo pognali ukaz
C-x C-b, in vsebuje seznam delovnih podro�ij. Tudi delovno podro�je
,,Messages`` ne pripada nobeni datoteki, ampak vsebuje sporo�ila, ki
jih je Emacs izpisoval v odzivnem podro�ju na dnu zaslona.

>> Vtipkajte C-x b *Messages* <Return> in si oglejte delovno podro�je
   s sporo�ili, zatem pa vtipkajte C-x b TUTORIAL <Return> in se tako
   vrnite v u�benik.

�e ste spreminjali besedilo ene datoteke, potem pa poiskali drugo, to
ne shrani spremeb v prvo datoteko. Te ostanejo znotraj Emacsa, na
delovnem podro�ju, ki pripada prvi datoteki. Ustvarjenje ali
spreminjanje delovnega podro�ja druge datoteke nima nobenega vpliva na
podro�je prve. To je zelo uporabno, pomeni pa tudi, da potrebujemo
udobno pot, da shranimo delovno podro�je prve datoteke. Nerodno bi
bilo preklapljanje na prvo podro�je s C-x C-f, da bi shranili s C-x
C-s. Namesto tega imamo:

	C-x s     Shrani nekatera delovna podro�ja

Ukaz C-x poi��e delovna podro�ja, katerih vsebina je bila spremenjena,
odkar je bila zadnji� shranjena na datoteko. Za vsako tako delovno
podro�je C-x s vpra�a, �e ga �elite shraniti.


* RAZ�IRJEN NABOR UKAZOV
------------------------

�e mnogo, mnogo je ukazov Emacsa, ki bi zaslu�ili, da jih obesimo na
razne kontrolne in meta znake. Emacs se temu izogne z ukazom X (iz angl.
eXtend - raz�iriti), ki uvede ukaz iz raz�irjenega nabora. Dveh vrst je:

	C-x	Znakovna raz�iritev (angl. Character eXtend).
		Sledi mu en sam znak.
	M-x	Raz�iritev s poimenovanim ukazom. Sledi mu dolgo ime
		ukaza.

Tudi ti ukazi so na splo�no uporabni, ne uporabljamo pa jih tako
pogosto kot tiste, ki ste se jih �e nau�ili. Dva ukaza iz raz�irjenega
nabora �e poznamo: C-x C-f, s katerim poi��emo datoteko, in C-x C-s, s
katerim datoteko shranimo. �e en primer je ukaz, s katerim Emacsu
povemo, da �elimo kon�ati z delom iz iziti iz Emacsa. Ta ukaz je C-x
C-c (ne skrbite: preden kon�a, Emacs ponudi, da shrani vse spremenjene
datoteke).

Z ukazom C-z Emacs zapustimo samo *za�asno*, tako da lahko ob vrnitvi
nadaljujemo z delom, kjer smo ostali.

Na sistemih, ki to dopu��ajo, ukaz C-z izide iz Emacsa v ukazno
lupino, a ga ne kon�a - �e uporabljate ukazno lupino C, se lahko
vrnete z ukazom ,fg` ali splo�neje z ukazom ,,%emacs``.

Drugod ukaz C-z po�ene sekundarno ukazno lupino, tako da lahko
po�enete kak�en drug program in se kasneje vrnete v Emacs. V tem
primeru pravzaprav Emacsa ne zapustimo. Ukaz ,,exit`` v ukazni lupini
je navadno na�in, da zapremo sekundarno lupino in se vrnemo v Emacs.

Ukaz C-x C-c uporabimo, �e se nameravamo odjaviti s sistema. To je
tudi pravilen na�in za izhod iz Emacsa, �e je tega pognal program za
delo s po�to ali kak drug program, saj ta verjetno ne ve, kaj
napraviti z za�asno prekinjenim Emacsom. V vseh ostalih primerih pa,
�e se ne nameravate odjaviti s sistema, uporabite C-z, in se vrnite v
Emacs, ko bi radi spet urejali besedilo.

Ukazov C-x je veliko. Zaenkrat smo spoznali naslednje:

	C-x C-f		Poi��i datoteko.
	C-x C-s		Shrani datoteko.
	C-x C-b		Prika�i seznam delovnih podro�ij.
	C-x C-c		Kon�aj Emacs.
	C-x 1		Zapri vsa okna razen enega.
	C-x u		Preklic zadnjega ukaza.

Poimenovani raz�irjeni ukazi so ukazi, ki se uporabljajo �e bolj
poredko, ali pa se uporabljajo samo v nekaterih na�inih dela.  Eden
takih je na primer ukaz replace-string, ki po vsem besedilu zamenja en
niz znakov z drugim. Ko vtipkate M-x, se to izpi�e v pogovornem
vmesniku na dnu zaslona, Emacs pa �aka, da vtipkate ime ukaza, ki ga
�elite priklicati; v tem primeru je to ,,replace-string``. Vtipkajte
samo ,,repl s<TAB>`` in Emacs bo dopolnil ime (<TAB> je tabulatorska
tipka; navadno jo najdemo nad tipko Caps Lock ali Shift na levi strani
tipkovnice). Ukaz vnesete s pritiskom na <Return>.

Ukaz replace-string potrebuje dva argumenta -- niz, ki ga �elite
zamenjati, in niz, s katerim bi radi zamenjali prvega. Vsakega posebej
vnesete in zaklju�ite s pritiskom na tipko Return.

>> Premaknite kazal�ek na prazno vrstico dve vrstici pod to, zatem
   vtipkajte M-x repl s<Return>zamenjala<Return>spremenila<Return>.

   Opazite, kako se je ta vrstica zamenjala? Vse besede
   z-a-m-e-n-j-a-l-a od tod do konca besedila ste nadomestili z besedo
   ,,spremenila``.


* AVTOMATI�NO SHRANJEVANJE
--------------------------

Spremembe v datoteki, ki jih �e niste shranili na disk, so izgubljene,
�e medtem denimo zmanjka elektrike. Da bi vas zavaroval pred tem,
Emacs periodi�no avtomati�no shrani vse datoteke, ki jih
urejate. Avtomati�no shranjena datoteka se od izvorne razlikuje po
znaku ,#` na za�etku in koncu imena: �e se je va�a datoteka imenovala
,,hello.c``, se avtomati�no shranjena datoteka imenuje
,,#hello.c#``. Ko normalno shranite datoteko, avtomati�no shranjena
datoteka ni ve� potrebna, in Emacs jo pobri�e.

�e res pride do izgube podatkov v pomnilniku, lahko povrnete avtomati�no
shranjeno besedilo tako, da normalno poi��ete datoteko (pravo ime
datoteke, ne ime avtomati�no shranjene datoteke), zatem pa vtipkate M-x
recover file<Return>. Ko vas vpra�a za potrditev, vtipkajte yes<Return>
za nadaljevanje in povrnitev avtomati�no shranjenenih podatkov.


* ODZIVNO PODRO�JE
------------------

Kadar Emacs opazi, da po�asi vtipkavate ukaz, odpre v zadnji vrstici
na dnu zaslona odzivno podro�je in v njem sproti prikazuje natipkano.


* STATUSNA VRSTICA
------------------

Vrstica nad odzivnim podro�jem je statusna vrstica. Ta ka�e verjetno
nekaj podobnega kot:

--:** TUTORIAL          (Fundamental)--L670--58%----------------------

V njej so izpisani pomembni podatki o stanju Emacsa in besedilu, ki ga
urejate.

Zdaj �e veste, kaj pomeni ime datoteke -- to je datoteka, ki ste jo
poiskali. Oznaka --NN%-- pomeni, da je nad vrhom zaslona �e NN
odstotkov celotne datoteke. �e je za�etek datoteke na zaslonu, bo
namesto --00%-- pisalo --Top--. Podobno bo pisalo --Bot--, �e je
zadnja vrstica datoteke na zaslonu. �e je datoteka, ki jo ogledujete,
tako kratka, da gre vsa na en zaslon, pa bo pisalo --All--.

�rka L in �tevilke za njo ka�ejo polo�aj �e druga�e, kot zaporedno
�tevilko vrstice, v kateri je kazal�ek.

Zvezdice na za�etku vrstice pomenijo, da ste datoteko �e spreminjali.
Tik po tem, ko ste odprli ali shranili datoteko, ni nobenih zvezdic,
so samo �rtice.

Del statusne vrstice znotraj oklepajev vam pove, v kak�nem na�inu dela
Emacs. Privzeti na�in je osnovni na�in (Fundamental), v katerem ste
sedaj. Fundamental je eden od glavnih na�inov (angl. major
mode). Emacs pozna veliko razli�nih glavnih na�inov. Nekateri od njih
so namenjeni pisanju programov, kot na primer Lisp, ali pisanju
besedil, kot npr. Text. Naenkrat je lahko aktiven le en glavni na�in,
njegovo ime pa je vedno izpisano v statusni vrstici, kjer zdaj pi�e
Fundamental.

Glavni na�ini lahko spremenijo pomen nekaterim ukazom. Obstajajo,
denimo, ukazi za pisanje komentarjev v programu, in ker ima vsak
programski jezik svoje predstave o tem, kako mora komentar izgledati,
mora vsak glavni na�in vnesti komentarje druga�e. Ker je vsak glavni
na�in ime raz�irjenega ukaza, lahko tako tudi izbiramo glavni
na�in. Na primer, M-x fundamental-mode vas postavi v na�in
Fundamental.

�e nameravate popravljati slovensko (ali angle�ko) besedilo, kot je na
primer tole, boste verjetno izbrali tekstovni na�in (Text).
>> Vtipkajte M-x text mode<Return>.

Brez skrbi, noben od ukazov Emacsa, ki ste se jih nau�ili, se s tem ne
spremeni kaj dosti. Lahko pa opazite, da Emacs zdaj jemlje opu��aje za
dele besed, ko se premikate z M-f ali M-b. V osnovnem na�inu jih je
obravnaval kot meje med besedami.

Glavni na�ini navadno po�enjajo majhne spremembe, kot je ta: ve�ina
ukazov ,,opravi isti posel``, vendar pa to po�nejo na razli�en na�in.

Dokumentacijo o trenutno aktivnem glavnem na�inu dobite z ukazom C-h m.

>> Uporabite C-u C-v enkrat ali ve�krat, toliko, da bo ta vrstica blizu
   vrha zaslona.
>> Vtipkajte C-h m, da vidite, v �em se tekstovni na�in (Text) razlikuje
   od osnovnega (Fundamental).
>> Vtipkajte C-x 1, da umaknete dokumentacijo z zaslona.

Glavnim na�inom pravimo glavni na�ini zato, ker obstajajo tudi
podna�ini (angl. minor modes). Podna�ini ne nadome��ajo glavnih
na�inom, ampak le spreminjajo njihovo obna�anje. Podna�ine lahko
aktiviramo ali deaktiviramo neodvisno od glavnega na�ina in neodvisno
od ostalih podna�inov. Tako lahko ne uporabljate nobenega podna�ina,
en podna�in, ali kombinacijo ve�ih podna�inov.

Podna�in, ki je zelo uporaben posebno za pisanje besedil, je Auto
Fill. Ko je vklopljen, Emacs med pisanjem avtomati�no deli vrstice na
presledkih med besedami, tako da vrstice niso predolge.

Vklopite ga lahko z ukazom M-x auto fill mode<Return>. Ko je
vklopljen, ga lahko izklopite z istim ukazom, M-x
auto fill mode<Return>. Z istim ukazom torej preklapljamo
(angl. toggle) med vklopljenim in izklopljenim stanjem.

>> Vtipkajte zdaj M-x auto fill mode<Return>. Potem za�nite tipkati
   "asdf asdkl sdjf sdjkf"... dokler ne opazite, da je Emacs razbil
   vrstico na dve.  Med tipkanjem mora biti dovolj presledkov, saj
   Auto Fill prelamlja vrstice samo na presledkih.

�irina besedila je navadno postavljena na 70 znakov, kar pa lahko
spremenite z ukazom C-x f. Novo �irino morate podati kot �tevil�ni
argument.

>> Vtipkajte C-x f in argument 20. (C-u 2 0 C-x f). Zatem vtipkajte
   nekaj besedila in poglejte, �e bo Emacs res delil vrstice pri 20
   znakih. Potem z ukazom C-x f postavite mejo nazaj na 70.

Auto Fill deluje le, kadar pi�ete novo besedilo, ne pa,
kadar popravljate �e napisan odstavek.
Tak odstavek lahko poravnate tako, da kazal�ek premaknete nekam
znotraj odstavka in uka�ete M-q (META-q).

>> Premaknite kazal�ek v prej�nji odstavek in izvedite M-q.


* ISKANJE
---------

Emacs lahko v besedilu poi��e niz znakov (zaporedje znakov ali besed),
naprej ali nazaj po besedilu. Iskanje spada v skupino ukazov za
premikanje kazal�ka, saj premakne kazal�ek na kraj v besedilu, kjer je
na�el iskani niz.

Iskanje v Emacsu je morda nekoliko druga�no od tistega, ki ste ga
navajeni, in sicer je ,,inkrementalno``. To pomeni, da se iskanje
odvija hkrati s tem, ko tipkate iskani niz.

Ukaza za iskanje sta C-s za iskanje naprej po datoteki in C-r za
iskanje nazaj po datoteki. PO�AKAJTE! Ne preizku�ajte jih �e ta hip!

Ko boste natipkali C-s, boste opazili niz ,,I-search`` kot pozivnik
v pogovornem vmesniku. To vam pove, da je Emacs v inkrementalnem iskanju
in vas �aka, da za�nete tipkati, kar i��ete. <Return> zaklju�i iskanje.

>> Pritisnite zdaj C-s. PO�ASI, �rko za �rko, vtipkajte besedo
   ,,kazal�ek``. Za vsako vtipkano �rko se ustavite in si oglejte, kaj
   se je zgodilo s kazal�kom.
>> �e enkrat pritisnite C-s, da poi��ete naslednji ,,kazal�ek``.
>> �estkrat pritisnite <Delback> in opazujte, kako se premika kazal�ek.
>> Kon�ajte iskanje s tipko <Return>.

Ste videli, kaj se je zgodilo? Emacs pri inkrementalnem iskanju sku�a
poiskati niz, ki ste ga natipkali do tistega hipa. Da poi��ete
naslednje mesto, kjer se pojavi ,,kazal�ek``, samo �e enkrat
pritisnete C-s. �e takega mesta ni, Emacs �ivkne in vam sporo�i, da
iskanje ni uspelo. Tudi C-g prekine iskanje.

OPOZORILO: Na nekaterih sistemih bo s pritiskom na C-s ekran
zmrznil. To je znak, da je operacijski sistem prestregel znak C-s in
ga interpretiral kot znak za prekinitev toka podatkov, namesto da bi
ga posredoval programu Emacs. Ekran ,,odtajate`` s pritiskom na
C-q. Potem si oglejte razdelek ,,Spontaneous Entry to Incremental
Search`` v priro�niku za nasvet, kako se spopasti s to nev�e�nostjo.

�e sredi inkrementalnega iskanja pritisnete <Delback>, boste opazili,
da to pobri�e zadnji znak v iskanem nizu, kazal�ek pa se premakne
nazaj na mesto v besedilu, kjer je na�el kraj�i niz. Na primer,
predpostavimo, da ste do zdaj natipkali ,,ka`` in je kazal�ek na
mestu, kjer se prvi� pojavi ,,ka``. �e zdaj pritisnete <Delback>, boste
s tem v pogovornem vmesniku izbrisali ,a`, hkrati pa se bo kazal�ek
postavil na mesto, kjer je prvi� na�el ,k`, preden ste natipkali �e
,a`.

�e sredi iskanja vtipkate katerikoli kontrolni znaki ali metaznak
(razen tistih, ki imajo poseben pomen pri iskanju, to sta C-s in C-r),
se iskanje prekine.

C-s za�ne iskati na mestu v datoteki, kjer trenutno stoji kazal�ek, in
i��e do konca datoteke. �e bi radi iskali proti za�etku datoteke,
namesto C-s vtipkamo C-r.  Vse, kar smo povedali o ukazu C-s, velja
tudi za C-r, le smer iskanja je obrnjena.


* VE� OKEN NA ZASLONU
---------------------

Ena simpati�nih lastnosti Emacsa je, da zna hkrati prikazati ve� oken
na ekranu, tudi �e ne delamo v grafi�nem na�inu.

>> Premaknite kazal�ek v to vrstico in vtipkajte C-u 0 C-l (zadnji
   znak je CONTROL-L, ne CONTROL-1)
>> Zdaj vtipkajte C-x 2, da razdelite zaslon na dve okni.
   V obeh oknih imate odprt ta priro�nik. Kazal�ek je ostal v zgornjem
   oknu.
>> Pritisnite C-M-v za listanje v spodnjem oknu.
   (�e nimate tipke META, tipkajte ESC C-v).
>> Vtipkajte C-x o (o kot ,,other``, drugi), da preselite kazal�ek v
   spodnje okno.
>> S C-v in M-v se v spodnjem oknu premikate po vsebini datoteke.
   Zgornje okno �e vedno ka�e ta navodila.
>> Ponovni C-x o vas vrne v zgornje okno. Kazal�ek se je vrnil na
   mesto, kjer je bil, preden smo sko�ili v spodnje okno.

Z ukazom C-x o lahko preklapljamo med okni. Vsako okno si zapomni, kje
v oknu je ostal kazal�ek, samo trenutno aktivno okno pa kazal�ek tudi
v resnici prika�e. Vsi obi�ajni ukazi za urejanje, ki smo se jih
nau�ili, veljajo za aktivno okno.

Ukaz C-M-v je zelo uporaben, kadar urejamo besedilo v enem oknu,
drugega pa uporabljamo samo za pomo�. Kazal�ek ostaja ves �as v oknu,
v katerem urejamo, po vsebini spodnjega okna pa se vseeno lahko
premikamo, ne da bi morali venomer skakati iz enega okna v drugega.

C-M-v je primer znaka CONTROL-META. �e imate v resnici tipko META (na
PC navadno levi Alt), lahko vtipkate C-M-v tako, da dr�ite pritisnjeni
tako CONTROL kot META, medtem ko vtipkate v. Ni pomembno, katero od
tipk, CONTROL ali META, pritisnete prvo, saj obe delujeta �ele, ko
pritisnete znak, ki sledi (v zgornjem primeru ,v`).

Nasprotno pa je vrstni red pritiskanja pomemben, �e nimate tipke META
in namesto nje uporabljate ESC. V tem primeru morate najprej
pritisniti ESC, potem pa Control-v. Obratna kombinacija, CONTROL-ESC v
ne deluje. To je zato, ker je ESC znak sam po sebi, ne pa modifikator,
kot sta CONTROL in META.

>> V zgornjem oknu vtipkajte C-x 1, da se znebite spodnjega okna.

(�e bi vtipkali C-x 1 v spodnjem oknu, bi se znebili
zgornjega. Razmi�ljajte o tem ukazu kot ,,Obdr�i samo eno okno, in
sicer tisto, v katerem sem zdaj.``)

Seveda ni nujno, da obe okni ka�eta isto delovno podro�je. �e v enem
oknu izvedete C-x C-f in poi��ete novo datoteko, se vsebina drugega
okna ne spremeni. V vsakem oknu lahko neodvisno obdelujete drugo
datoteko.

Pa �e ena pot, kako v dveh oknih prika�ete dve razli�ni datoteki:

>> Vtipkajte C-x 4 C-f, in na pozivnik vtipkajte ime ene va�ih
   datotek. Kon�ajte z <Return>. Odpre se �e eno okno in izbrana
   datoteka se pojavi v drugem oknu. Tudi kazal�ek se preseli v drugo
   okno.

>> Vtipkajte C-x o, da se vrnete nazaj v zgornje okno, in C-x 1, da
   zaprete spodnje okno.


* REKURZIVNI NIVOJI UREJANJA
----------------------------

V�asih boste pri�li v nekaj, �emur se pravi ,,rekurzivni nivo
urejanja``. To se vidi po tem, da v statusni vrstici oglati oklepaji
oklepajo ime glavnega na�ina. V osnovnem na�inu bi, na primer, videli
[(Fundamental)] namesto (Fundamental).

Iz rekurzivnega nivoja urejanja se re�ite, �e vtipkate ESC ESC ESC. To
zaporedje je vsenamenski ukaz ,,pojdi ven``. Uporabite ga lahko tudi
za ukinjanje odve�nih oken, ali vrnitev iz pogovornega vmesnika.

>> Pritisnite M-x, da odprete pogovorni vmesnik, zatem pa vtipkajte
   ESC ESC ESC, da pridete ven iz njega.

Z ukazom C-g ne morete iz rekurzivnega nivoja urejanja, ker C-g
prekli�e ukaze ali argumente ZNOTRAJ rekurzivnega nivoja.


* DODATNA POMO�
---------------

V tem uvodu smo posku�ali zbrati dovolj informacij, da lahko za�nete
Emacs uporabljati. Emacs ponuja toliko, da bi bilo nemogo�e vse to
zbrati tukaj. Verjetno pa bi se vseeno radi nau�ili kaj o �tevilnih
koristnih mo�nostih, ki jih �e ne poznate. Emacs ima �e vgrajene
veliko dokumentacije, do katere lahko pridete s pritiskom na CONTROL-h
(h kot ,,help``, pomo�).

Za pomo� pritisnete C-h, potem pa vtipkate znak, ki pove, kak�no pomo�
�elite. �e ste poplnoma izgubljeni, vtipkajte C-h ? in Emacs vam bo
povedal, kak�na pomo� je sploh na voljo. �e ste vtipkali C-h, pa ste
si premislili, lahko ukaz prekli�ete s C-g.

(Na nekaterih sistemih se znak C-h preslika v kaj drugega. To ni
dobro, in v takem primeru se prito�ite sistemskemu vzdr�evalcu. Medtem
pa, �e C-h ne prika�e sporo�ila o pomo�i na dnu zaslona, namesto tega
poskusite pritisniti tipko F1 ali pa vtipkajte M-x help <Return>.)

Najosnovnej�i tip pomo�i prika�e C-h c. Pritisnite C-h, tipko c, zatem
pa ukazni znak ali zaporedje ukaznih znakov, in Emacs bo izpisal
kratek opis ukaza.

>> Vtipkajte C-h c C-p.
   Izpi�e se nekaj takega kot

	C-p runs the command previous-line

Ukaz je izpisal ime funkcije, ki izvede ukaz. Imena funkcij
uporabljamo, kadar pi�emo prilagoditve in raz�iritve Emacsa. Ker pa so
navadno imena funkcij izbrana tako, da kaj povedo o tem, kaj funkcija
po�ne, bo verjetno to tudi dovolj za kratko osve�itev, �e ste se z
ukazom �e kdaj sre�ali.

Ukazu C-h lahko sledi tudi zaporedje znakov, kot na primer C-x C-s,
ali, �e nimate tipke META, <Esc>v.

Za ve� informacij o ukazu vtipkajte C-h k namesto C-h c.

>> Vtipkajte C-h k C-p.

To odpre novo okno in v njem prika�e dokumentacijo o funkciji, obenem
z njenim imenom. Ko ste opravili, vtipkajte C-x 1, da se znebite okna
z pomo�jo. Tega seveda ni potrebno napraviti takoj, ampak lahko
urejate, medtem ko imate odprto okno s pomo�jo, in ga zaprete, ko ste
kon�ali.

Sledi �e nekaj uporabnih mo�nosti, ki jih ponuja pomo�:

   C-h f	Opi�i funkcijo. Kot argument morate podati ime
		funkcije.

>> Poskusite C-h f previous-line<Return>.
   To izpi�e vse podatke, ki jih ima Emacs o funkciji, ki izvede ukaz C-p.

Podoben ukaz C-h v izpi�e dokumentacijo za spremenljivke, s katerimi
lahko nastavite obna�anje Emacsa. Ob pozivniku morate vpisati ime
spremenljivke.

   C-h a	 Apropos. Vtipkajte klju�no besedo in Emacs bo izpisal
		 vse ukaze, ki vsebujejo to klju�no besedo. Vse te
		 ukaze lahko prikli�ete z META-x. Pri nekaterih ukazih
		 bo Apropos izpisal tudi eno ali dvoznakovno
		 zaporedje, s katerim dose�ete isti u�inek.

>> Vtipkajte C-h a file<Return>.

To odpre novo okno, v katerem so vsa dolga imena ukazov, ki vsebujejo
,,file`` v imenu. Izvedete jih lahko z M-x. Pri nekaterih se izpi�e
tudi kratek ukaz, npr. C-x C-f ali C-x C-w pri ukazih find-file in
write-file.

>> Pritisnite C-M-v, da se sprehajate po oknu s pomo�jo. Poskusite
   nekajkrat.

>> Vtipkajte C-x 1, da zaprete okno s pomo�jo.

   C-h i         Priro�niki z navodili za uporabo (tkim. datoteke
		 "info"). Ta ukaz vas prestavi v posebno delovno
		 podro�je, imenovano "info". V njem lahko prebirate
		 priro�nike za programe, ki so name��eni v sistemu. Z
		 ukazom m emacs<Return> denimo dobite priro�nik za
		 urejevalnik Emacs. �e sistema Info �e niste
		 uporabljali, vtipkajte ? in Emacs vas bo popeljal na
		 v�deni izlet po na�inu Info in mo�nostih, ki jih
		 ponuja. Ko boste zaklju�ili z branjem tega prvega
		 berila, bo priro�nik za Emacs v sistemu Info va�
		 glavni vir dokumentacije.


* DRUGE MO�NOSTI
----------------

�e ve� se lahko nau�ite o Emacsu z branjem priro�nika, bodisi
natisnjenega, bodisi na zaslonu v sistemu Info (uporabite menu Help
ali vtipkajte F10 h r). Dve mo�nosti, ki vam bosta morda posebej v�e�,
sta samodejno zaklju�evanje vrstice, s katerim prihranite nekaj
tipkanja, in dired, s katerim poenostavimo delo z datotekami.

Samodejno zaklju�evanje vrstic je na�in, s katerim prihranimo nekaj
tipkanja. �e �elite denimo preklopiti v delovno podro�je *Messages*,
je dovolj, da vtipkate C-x b *M<Tab> in Emacs bo sam dopolnil
preostanek imena delovnega podro�ja. Samodejno zaklju�evanje je
opisano v sistemu Info v priro�niku za Emacs, razdelek ,,Completion``.

Dired omogo�a izpis seznama datotek v imeniku (in po mo�nosti tudi
podimenikih), premikanje po seznamu, obiskovanje (odpiranje),
preimenovanje, brisanje in druge operacije z datotekami. Dired je
opisav v sistemu Info  v priro�niku za Emacs, razdelek ,,Dired``.

Priro�nik opisuje tudi mnoge druge mo�nosti Emacsa.


* ZAKLJU�EK
-----------

Zapomnite si, da Emacs zapustite z ukazom C-x C-c. �e bi radi samo
za�asno sko�ili v ukazno lupino in se kasneje vrnili v Emacs, pa
storite to z ukazom C-z.

Ta u�benik je napisan z namenom, da bi bil razumljiv vsem novincem v
Emacsu. �e se vam kaj ne zdi jasno napisano, ne valite krivde nase -
prito�ite se!


* RAZMNO�EVANJE IN RAZ�IRJANJE
------------------------------

Angle�ki izvirnik tega uvoda v Emacs je naslednik dolge vrste tovrstnih
besedil, za�en�i s tistim, ki ga je Stuart Cracraft napisal za izvorni
Emacs. V sloven��ino ga je prevedel Primo� Peterlin.

To besedilo, kot sam GNU Emacs, je avtorsko delo, in njegovo
razmno�evanje in raz�irjanje je dovoljeno pod naslednjimi pogoji:

Copyright (C) 1985, 1996, 1998, 2001, 2002, 2003, 2004,
   2005, 2006, 2007  Free Software Foundation, Inc.

   Dovoljeno je izdelovati in raz�irjati neokrnjene kopije tega spisa
   v kakr�nikoli obliki pod pogojem, da je ohranjena navedba o
   avtorstvu in to dovoljenje, ter da distributer dovoljuje prejemniku
   nadaljnje raz�irjanje pod pogoji, navedenimi v tem dovoljenju.

   Pod pogoji iz prej�njega odstavka je dovoljeno raz�irjati
   spremenjene verzije tega spisa ali njegovih delov, �e je jasno
   ozna�eno, kdo je nazadnje vnesel spremembe.

Pogoji za razmno�evanje in raz�irjanje samega Emacsa so malo druga�ni,
a v istem duhu. Prosimo, preberite datoteko COPYING in potem dajte
kopijo programa GNU Emacs svojim prijateljem. Pomagajte zatreti
obstrukcionizem (,,lastni�tvo``) v programju tako, da uporabljate,
pi�ete in delite prosto programje!

;;; Local Variables:
;;; coding: iso-latin-2
;;; sentence-end-double-space: nil
;;; End:

;;; arch-tag: 985059e4-44c6-4ac9-b627-46c8db57acf6
