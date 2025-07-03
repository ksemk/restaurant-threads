Scenariusz działania aplikacji emulującej restaurację (zarządzanie wielowątkowe)
1. Sala restauracyjna – przyjmowanie gości
   Restauracja posiada określoną liczbę stolików o różnych rozmiarach (np. 2-, 4-, 6-osobowe).

Do restauracji przychodzą grupy klientów (wątki), które chcą zostać obsłużone.

Jeśli znajdzie się odpowiedni wolny stolik (rozmiar >= liczba osób w grupie), grupa jest sadzana.

Jeśli nie ma odpowiedniego stolika, grupa trafia do kolejki oczekujących (np. kolejka FIFO).

Po zajęciu stolika, grupa składa zamówienie u przypisanego kelnera.

2. Kelnerzy (n wątków)
   Każdy kelner może obsługiwać wiele grup, ale tylko jedną naraz.

Odbiera zamówienie od stolika i przekazuje je do kuchni (wspólna kolejka zamówień).

Po odebraniu gotowego zamówienia z kuchni, serwuje posiłek, ale dopiero wtedy, gdy cały zestaw dań dla grupy jest gotowy.

Po skończonym posiłku grupa klientów opuszcza restaurację, a stolik jest oznaczany jako wolny.

3. Kuchnia – realizacja zamówień
   W kuchni zamówienia trafiają do wspólnej kolejki. Następnie są dzielone na komponenty:

Główny kucharz – dania główne

Kucharz rybny – dania rybne

Piekarz – pieczywo

(Można dodać kucharza deserowego lub sałatkowego)

Piekarz działa cały czas i piecze chleb na zapas, ale tylko do określonego limitu magazynowego.

Każdy kucharz (osobny wątek) pobiera swoją część zamówień i przygotowuje dania (symulacja czasu np. sleep).

Po przygotowaniu, dania są buforowane per stolik. Kelner może je odebrać dopiero, gdy wszystkie części zestawu są gotowe.

Mechanizmy synchronizacji i struktury danych
Muteksy/semafory/monitory do synchronizacji dostępu do stolików, kolejki zamówień i kuchni.

Kolejki blokujące dla oczekujących klientów i zamówień w kuchni.

Mapa stolików z informacją o dostępności (np. Map<Stolik, boolean>).

Bufory per zamówienie/grupę klientów, synchronizujące moment, gdy wszystkie dania są gotowe.

Limitowany magazyn pieczywa – np. BlockingQueue z maksymalną pojemnością.