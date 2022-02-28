COMAN ROBERT
323 CB

1. REPREZENTAREA DATELOR IN SERVER
Pentru a putea reprezenta datele in server, mai intai am definit un tip de lista
simplu inlantuita generica. Apoi, am implementat functii de baza pentru a face
operatii pe aceste liste (remove, cons, head, destroy, etc). In memorie, pe durata
rularii serverului, se vor stoca 3 liste: lista de clienti, lista de topicuri si
lista de mesaje care trebuie trimise la cel putin un client (sf = 1).

1.1. CLIENTUL
Clientul are ca structura ID-ul (sir de caractere), socketul curent (daca este
offline, socketul curent va avea valoarea -1), statusul (online sau offlines),
o lista de pointeri catre mesajele neprimite si o lista de pointeri catre
topicurile la care a dat subscribe. Am ales sa adaug clientului aceasta lista
de pointeri catre topicuri, deoarece atunci cand caut un topic dupa un client,
operatia poate fi mai eficienta ca timp. Lista de clienti este o lista de pointeri
catre un element de tip client. Cand un client se reconecteaza, verific daca
lista catre mesajele neprimite este nenula si daca da atunci ii trimit toate
mesajele neprimite inca.

1.2 TOPICUL
Topicul are ca structura numele (sir de caractere) si o lista de pointeri. Fiecare
pointer din aceasta lista pointeaza catre adresa unui wrapper peste client si
parametrul SF (struct client_sf). Un wrapper de acest tip contine un pointer
catre client si valoarea sf-ului.

1.3 MESAJELE NEPRIMITE
Pentru a trata aceasta situatie, am definit o structura care este un wrapper
peste mesajul propriu zis care va fi detaliat in sectiunea urmatoare si numarul
de clienti care mai trebuie sa il primeasca. Acest obiect este stocat intr-o
lista, atunci cand exista cel putin un client care nu l-a primit, iar cand
numarul de clienti (msg->nr_clients) devine 0, atunci mesajul este eliberat din
memorie, deoarece a fost trimis la toti clientii si nu mai este nevoie de el.

===============================================================================
2. EXPLICAREA PROTOCOLULUI
Mesajul care ajunge la clientul TCP de la server este de tipul "struct msg". Acest
tip contine un camp de un octet care indica tipul mesajului (WRONG - daca clientul
trimite un mesaj nerecunoscut de server, o comanda gresita; ACK - dupa ce clientul
trimite mesaj de sub / unsub, serverul ii trimite inapoi confirmarea ca s-a realizat
operatia dorita, iar clientul afiseaza acest lucru; TOPIC_NEWS - cand clientul
primeste un mesaj obisnuit pe un anumit topic; KILL - cand serverul ii da clientului
comanda sa se inchida, asta poate intampla cand se da comanda exit la server). Pe langa
acest octet, mesajul contine o structura de tip udp_header, unde se afla adresa IP si
portul clientului UDP. Apoi se afla mesajul propriu zis (struct udp_msg), care contine
topicul, id si payload conform enuntului.
Pe server, am deschis socketii de udp si tcp + citirea de la tastatura. Cand sunt pe
socketul tcp, inseamna ca s-a deschis conexiunea catre un client care poate fi unul
nou sau unul care s-a mai conectat si inainte. In ambele cazuri, primul mesaj pe
care clientul il trimite serverului dupa ce s-a conectat contine ID-ul acestuia. Pe
baza lui, serverul il cauta in lista de clienti, si daca nu il gaseste il adauga.
Daca clientul exista deja in lista si este online, atunci clientul actual nu se poate
conecta si i se transmite un mesaj de tip KILL. Altfel se recalculeaza fd_max.
Pe socketul_udp, se primeste un mesaj si se trimite la toti clientii abonati,
daca este cel putin un client cu sf == 1 si offline abonat la acel topic, mesajul
se salveaza. In cazul in care ne aflam pe socketul asociat unui client tcp, inseamna
ca trebuie sa primim un mesaj de la client fie de abonare, fie de dezabonare. Daca
primim unul din aceste 2 tipuri de mesaje, ii trimitem inapoi confirmarea, iar daca
mesajul este gresit, clientul primeste un mesaj de tip WRONG. De asemenea, daca nr
de biti primiti este zero, atunci clientul s-a deconectat si se afiseaza acest lucru.
In subscriber.c, primesc mesaj de la server si verific tipul. Daca este WRONG, nu fac
nimic, daca este ACK afisez mesajul de confirmare (Subscribed sau Unsubscribed), daca
este KILL, opresc programul, iar daca este de tip TOPIC_NEWS, procesez mesajul si
afisez informatia asa cum este specificat in enunt.