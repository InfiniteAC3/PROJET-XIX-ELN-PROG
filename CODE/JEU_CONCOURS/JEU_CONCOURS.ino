/*
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║         Q U E S T I O N S   P O U R   U N   C H A M P I O N    ║
 * ║         Simulation Arduino UNO + MAX7219 – 8 afficheurs 7-seg   ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  CÂBLAGE MATÉRIEL                                                ║
 * ║  ─────────────────────────────────────────────────────────────  ║
 * ║  MAX7219   : DIN=11 | CLK=13 | CS/LOAD=10                       ║
 * ║  Buzzers   : Joueur A=7  | B=8  | C=9  | D=12                   ║
 * ║  Boutons   : ROK=2 (valider) | RNOK=3 (invalider)               ║
 * ║              [+]=0 (incrémenter) | [-]=1 (décrémenter)           ║
 * ║  LED bic.  : Anode=4 (vert) | Cathode=5 (rouge)                 ║
 * ║  HP/Buzzer : Pin=6 (digital / tone)                              ║
 * ║                                                                  ║
 * ║  DÉROULEMENT                                                     ║
 * ║  ─────────────────────────────────────────────────────────────  ║
 * ║  1. Arbitre choisit le nombre de joueurs (2-4) → ROK             ║
 * ║  2. Avant chaque question, arbitre règle :                       ║
 * ║       TEMPS   (5 à 60 s, pas de 5)  → ROK                       ║
 * ║       Pts OK  (1 à 20 pts)          → ROK                       ║
 * ║       Pts NOK (0 à 10 pts pénalité) → ROK                       ║
 * ║  3. Chrono démarre : "----|  15" affiché et décompte            ║
 * ║  4. Joueur qui buzzé : affiche "X | 12" (lettre + temps restant) ║
 * ║  5. Arbitre appuie ROK (vert + bip joyeux) ou RNOK (rouge + bip)║
 * ║  6. Score du joueur actif → puis défilement de tous les scores  ║
 * ║  7. Maintenir [-] ≥ 2 s pendant le chrono = FIN DE PARTIE       ║
 * ║  8. Classement final (rang + score), animation du vainqueur      ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#include <LedControl.h>

// ══════════════════════════════════════════════════════════════════
//  BROCHES
// ══════════════════════════════════════════════════════════════════
#define DIN_PIN    11
#define CLK_PIN    13
#define LOAD_PIN   10

#define BTN_OK     2    // Arbitre : valider (bonne réponse / confirmer menu)
#define BTN_NOK    3    // Arbitre : invalider (mauvaise réponse)
#define BTN_PLUS   0    // Arbitre : [+] incrémenter
#define BTN_MOINS  1    // Arbitre : [-] décrémenter | maintenu 2 s = FIN

#define BJ_A    7       // Buzzer joueur A
#define BJ_B    8       // Buzzer joueur B
#define BJ_C    9       // Buzzer joueur C
#define BJ_D   12       // Buzzer joueur D

#define BUZZER  6       // Haut-parleur (digital / tone())
#define LED_A   4       // Anode  LED bicolore  → vert quand HIGH
#define LED_C   5       // Cathode LED bicolore → rouge quand HIGH

// ══════════════════════════════════════════════════════════════════
//  OBJET MAX7219
// ══════════════════════════════════════════════════════════════════
LedControl lc = LedControl(DIN_PIN, CLK_PIN, LOAD_PIN, 1);

// ══════════════════════════════════════════════════════════════════
//  STRUCTURE JOUEUR
// ══════════════════════════════════════════════════════════════════
struct Joueur {
  char lettre;   // 'A', 'B', 'C', 'D'
  int  score;    // Score courant (peut être négatif)
  int  ok;       // Nb de bonnes réponses
  int  nok;      // Nb de mauvaises réponses
};

// ══════════════════════════════════════════════════════════════════
//  VARIABLES GLOBALES
// ══════════════════════════════════════════════════════════════════
Joueur        J[4];
int           nbJ    = 4;   // Nombre de joueurs actifs (2-4)
int           actif  = -1;  // Index du joueur ayant buzzé (-1 = personne)

// Pins buzzers dans l'ordre A, B, C, D
const int BJ_PINS[4] = { BJ_A, BJ_B, BJ_C, BJ_D };

// Configuration par défaut (modifiable par arbitre avant chaque question)
int cfgT   = 15;  // Temps en secondes       (5 – 60, pas 5)
int cfgOK  = 10;  // Points bonne réponse    (1 – 20)
int cfgNOK =  5;  // Pénalité mauvaise réponse (0 – 10)

// ══════════════════════════════════════════════════════════════════
//  COUCHE AFFICHAGE 7 SEGMENTS
// ══════════════════════════════════════════════════════════════════

/**
 * Convertit un caractère en code 7-segments pour le MAX7219.
 */
byte seg7(char c) {
  switch (toupper(c)) {
    // ── Lettres ──
    case 'A': return 0x77;  case 'B': return 0x1F;
    case 'C': return 0x4E;  case 'D': return 0x3D;
    case 'E': return 0x4F;  case 'F': return 0x47;
    case 'G': return 0x5E;  case 'H': return 0x37;
    case 'I': return 0x30;  case 'J': return 0x3C;
    case 'K': return 0x37;  case 'L': return 0x0E;
    case 'M': return 0x76;  case 'N': return 0x25;
    case 'O': return 0x7E;  case 'P': return 0x67;
    case 'Q': return 0x73;  case 'R': return 0x05;
    case 'S': return 0x5B;  case 'T': return 0x0F;
    case 'U': return 0x3E;  case 'Y': return 0x3B;
    // ── Chiffres ──
    case '0': return 0x7E;  case '1': return 0x30;
    case '2': return 0x6D;  case '3': return 0x79;
    case '4': return 0x33;  case '5': return 0x5B;
    case '6': return 0x5F;  case '7': return 0x70;
    case '8': return 0x7F;  case '9': return 0x7B;
    // ── Symboles ──
    case '-': return 0x01;  case '_': return 0x08;
    case ' ': return 0x00;  default:  return 0x00;
  }
}

/** Efface les 8 digits. */
void clr() { lc.clearDisplay(0); }

/**
 * Affiche une chaîne de max 8 caractères.
 * Les positions non remplies sont éteintes.
 */
void aff(const char* s) {
  for (int i = 0; i < 8; i++)
    lc.setRow(0, i, (s[i] != '\0') ? seg7(s[i]) : 0x00);
}

/**
 * Formate un score signé sur 4 caractères.
 * Exemples :  35 → " 035"   |   -5 → "-005"
 */
void fmtSc(char* out4, int v) {
  int av = (v < 0) ? -v : v;
  if (av > 999) av = 999;
  snprintf(out4, 5, "%c%03d", (v < 0 ? '-' : ' '), av);
}

// ══════════════════════════════════════════════════════════════════
//  COUCHE SONORE
// ══════════════════════════════════════════════════════════════════

/** Joue une note de fréquence f (Hz) pendant ms millisecondes. */
void note(int f, int ms) {
  tone(BUZZER, f, ms);
  delay(ms + 25);
}

void bipMenu()  { note(700,  60); }                            // Navigation menu
void bipBuzz()  { note(1400, 70); delay(40); note(1400, 70); } // Joueur buzze
void bipOK()    { note(880, 110); note(1100, 190); }           // Bonne réponse
void bipNOK()   { note(350, 380); }                            // Mauvaise réponse
void bipTemps() { note(400, 200); note(300, 350); }            // Temps écoulé
void bipFin()   { note(1000,100); note(1200,100); note(1500,250); } // Fin de partie
void bipStart() { note(440,80); note(660,80); note(880,160); } // Démarrage GO

// ══════════════════════════════════════════════════════════════════
//  LED BICOLORE
// ══════════════════════════════════════════════════════════════════
void ledV() { digitalWrite(LED_A, HIGH); digitalWrite(LED_C, LOW);  } // Vert
void ledR() { digitalWrite(LED_A, LOW);  digitalWrite(LED_C, HIGH); } // Rouge
void ledX() { digitalWrite(LED_A, LOW);  digitalWrite(LED_C, LOW);  } // Éteinte

// ══════════════════════════════════════════════════════════════════
//  GESTION BOUTONS  (anti-rebond 60 ms)
// ══════════════════════════════════════════════════════════════════
static unsigned long tDb[14]   = {0};
static bool          bPrev[14] = {false};

/**
 * Retourne true UNE SEULE FOIS au moment du front descendant (appui).
 */
bool pressed(int pin) {
  bool cur = (digitalRead(pin) == LOW);
  if (cur && !bPrev[pin] && (millis() - tDb[pin] > 60)) {
    tDb[pin] = millis(); bPrev[pin] = true; return true;
  }
  if (!cur) bPrev[pin] = false;
  return false;
}

/**
 * Retourne true UNE SEULE FOIS après un maintien ≥ ms millisecondes.
 */
bool longPrsd(int pin, unsigned int ms) {
  static unsigned long t0[14]  = {0};
  static bool          hold[14] = {false};
  bool cur = (digitalRead(pin) == LOW);
  if (cur  && !hold[pin]) { t0[pin] = millis(); hold[pin] = true; }
  if (!cur)                 hold[pin] = false;
  if (hold[pin] && (millis() - t0[pin] > ms)) { hold[pin] = false; return true; }
  return false;
}

// ══════════════════════════════════════════════════════════════════
//  INITIALISATION JOUEURS
// ══════════════════════════════════════════════════════════════════
void initJ() {
  const char L[4] = {'A','B','C','D'};
  for (int i = 0; i < 4; i++) {
    J[i].lettre = L[i];
    J[i].score  = 0;
    J[i].ok     = 0;
    J[i].nok    = 0;
  }
}

// ══════════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════════
void setup() {
  // Entrées avec résistance de tirage interne
  int inp[8] = {BTN_OK, BTN_NOK, BTN_PLUS, BTN_MOINS,
                BJ_A, BJ_B, BJ_C, BJ_D};
  for (int i = 0; i < 8; i++) pinMode(inp[i], INPUT_PULLUP);

  // Sorties
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_A,  OUTPUT);
  pinMode(LED_C,  OUTPUT);
  ledX();

  // MAX7219 : réveil + luminosité
  lc.shutdown(0, false);
  lc.setIntensity(0, 12);
  clr();
  initJ();

  // ── Écran de bienvenue ─────────────────────────────────────────
  aff("  QPC   "); delay(900);
  aff("CHAMPION"); delay(1200);
  aff(" BIENVN "); delay(800);
  clr(); delay(300);
}

// ══════════════════════════════════════════════════════════════════
//  MENU ARBITRE 1 – Nombre de joueurs
//    [+] / [-]  pour ajuster   |   ROK pour confirmer
// ══════════════════════════════════════════════════════════════════
void menuNbJoueurs() {
  while (true) {
    char buf[9];
    snprintf(buf, 9, "JOUEUR %1d", nbJ);
    aff(buf);
    if (pressed(BTN_PLUS))  { nbJ++; if (nbJ > 4) nbJ = 2; bipMenu(); }
    if (pressed(BTN_MOINS)) { nbJ--; if (nbJ < 2) nbJ = 4; bipMenu(); }
    if (pressed(BTN_OK))    { bipMenu(); break; }
    delay(20);
  }
}

// ══════════════════════════════════════════════════════════════════
//  MENU ARBITRE 2 – Configuration de la question
//    Étape 1 : TEMPS   (5 à 60 s, pas de 5)
//    Étape 2 : Pts OK  (1 à 20)
//    Étape 3 : Pts NOK pénalité (0 à 10)
//    ROK pour passer à l'étape suivante, puis lancer le "GO !"
// ══════════════════════════════════════════════════════════════════
void menuQuestion() {
  // ─── Étape 1 : Temps ───────────────────────────────────────────
  while (true) {
    char buf[9]; snprintf(buf, 9, "TEMPS %2d", cfgT); aff(buf);
    if (pressed(BTN_PLUS))  { cfgT += 5; if (cfgT > 60) cfgT = 5;  bipMenu(); }
    if (pressed(BTN_MOINS)) { cfgT -= 5; if (cfgT < 5)  cfgT = 60; bipMenu(); }
    if (pressed(BTN_OK))    { bipMenu(); break; }
    delay(20);
  }
  // ─── Étape 2 : Points OK ───────────────────────────────────────
  while (true) {
    char buf[9]; snprintf(buf, 9, "OK    %2d", cfgOK); aff(buf);
    if (pressed(BTN_PLUS))  { cfgOK++; if (cfgOK  > 20) cfgOK  = 1;  bipMenu(); }
    if (pressed(BTN_MOINS)) { cfgOK--; if (cfgOK  < 1)  cfgOK  = 20; bipMenu(); }
    if (pressed(BTN_OK))    { bipMenu(); break; }
    delay(20);
  }
  // ─── Étape 3 : Pénalité NOK ────────────────────────────────────
  while (true) {
    char buf[9]; snprintf(buf, 9, "NOK   %2d", cfgNOK); aff(buf);
    if (pressed(BTN_PLUS))  { cfgNOK++; if (cfgNOK > 10) cfgNOK = 0;  bipMenu(); }
    if (pressed(BTN_MOINS)) { cfgNOK--; if (cfgNOK < 0)  cfgNOK = 10; bipMenu(); }
    if (pressed(BTN_OK))    { bipMenu(); break; }
    delay(20);
  }
  // ─── Lancement ─────────────────────────────────────────────────
  aff("  GO    "); bipStart(); delay(700);
}

// ══════════════════════════════════════════════════════════════════
//  AFFICHAGE SCORES INTERMÉDIAIRES  (défilement après chaque réponse)
// ══════════════════════════════════════════════════════════════════
void showScores() {
  aff("SCORES  "); delay(700);
  for (int i = 0; i < nbJ; i++) {
    char sc[5]; fmtSc(sc, J[i].score);
    char buf[9]; snprintf(buf, 9, "%c   %s", J[i].lettre, sc);
    aff(buf); delay(1400);
  }
}

// ══════════════════════════════════════════════════════════════════
//  CLASSEMENT FINAL
// ══════════════════════════════════════════════════════════════════
void showFin() {
  bipFin();
  aff("FIN JEU "); delay(1500);

  // ── Tri décroissant par score (bubble sort) ────────────────────
  Joueur tri[4];
  for (int i = 0; i < nbJ; i++) tri[i] = J[i];
  for (int i = 0; i < nbJ - 1; i++)
    for (int k = 0; k < nbJ - i - 1; k++)
      if (tri[k].score < tri[k+1].score) {
        Joueur tmp = tri[k]; tri[k] = tri[k+1]; tri[k+1] = tmp;
      }

  // ── Affichage rang par rang : "1A   035" ──────────────────────
  aff("CLASSMT "); delay(1000);
  for (int i = 0; i < nbJ; i++) {
    char sc[5]; fmtSc(sc, tri[i].score);
    char buf[9]; snprintf(buf, 9, "%d%c  %s", i + 1, tri[i].lettre, sc);
    aff(buf); delay(2500);
  }

  // ── Animation vainqueur (clignotement 6 fois) ─────────────────
  char win[9]; snprintf(win, 9, "WIN   %c ", tri[0].lettre);
  for (int i = 0; i < 7; i++) {
    (i % 2 == 0) ? aff(win) : clr();
    if (i % 2 == 0) bipOK();
    delay(380);
  }
  aff(win);
}

// ══════════════════════════════════════════════════════════════════
//  CŒUR DU JEU :  Traitement d'UNE question
//
//  Retourne  true  → continuer le jeu
//  Retourne  false → arbitre a demandé la fin de partie
// ══════════════════════════════════════════════════════════════════
bool uneQuestion() {

  menuQuestion();  // L'arbitre configure temps + points

  unsigned long tStart = millis();
  actif = -1;

  // ════════════════════════════════════════════════════════════════
  //  PHASE 1 – Attente du buzz  (chrono visible + clignotement urgence)
  // ════════════════════════════════════════════════════════════════
  while (actif < 0) {

    int restant = cfgT - (int)((millis() - tStart) / 1000UL);

    // ── Temps écoulé sans buzz ─────────────────────────────────
    if (restant <= 0) {
      aff("ECOULE  ");
      bipTemps();
      delay(1300);
      return true;
    }

    // ── Affichage dynamique "----  15" ─────────────────────────
    //    Clignotement urgence quand restant ≤ 5 s
    char buf[9];
    bool clignote = (restant <= 5) && ((millis() / 400) % 2 == 0);
    if (clignote)
      snprintf(buf, 9, "----    ");
    else
      snprintf(buf, 9, "----  %2d", restant);
    aff(buf);

    // ── Balayage des buzzers ────────────────────────────────────
    for (int i = 0; i < nbJ; i++) {
      if (digitalRead(BJ_PINS[i]) == LOW) { actif = i; break; }
    }

    // ── Long press [-] ≥ 2 s = Fin de partie ──────────────────
    if (longPrsd(BTN_MOINS, 2000)) return false;

    delay(40);
  }

  // ════════════════════════════════════════════════════════════════
  //  PHASE 2 – Joueur actif affiché, arbitre tranche
  //           Affiche "A     12" (lettre + temps restant)
  //           Clignotement du chiffre sous les 5 dernières secondes
  // ════════════════════════════════════════════════════════════════
  bipBuzz();

  // Temps restant au moment du buzz
  unsigned long tBuzz  = millis();
  int           tAvant = cfgT - (int)((tBuzz - tStart) / 1000UL);

  while (true) {

    int t = tAvant - (int)((millis() - tBuzz) / 1000UL);
    if (t < 0) t = 0;

    // Affichage : "A     12" avec clignotement urgence
    char buf[9];
    bool clignote = (t <= 5) && ((millis() / 300) % 2 == 0);
    if (clignote)
      snprintf(buf, 9, "%c       ", J[actif].lettre);
    else
      snprintf(buf, 9, "%c     %2d", J[actif].lettre, t);
    aff(buf);

    // ── ROK = bonne réponse ─────────────────────────────────────
    if (pressed(BTN_OK)) {
      J[actif].ok++;
      J[actif].score += cfgOK;
      char res[9]; snprintf(res, 9, "OK    %2d", cfgOK);
      aff(res); ledV(); bipOK(); delay(1700); ledX();
      break;
    }

    // ── RNOK = mauvaise réponse ─────────────────────────────────
    if (pressed(BTN_NOK)) {
      J[actif].nok++;
      J[actif].score -= cfgNOK;
      char res[9]; snprintf(res, 9, "NOK   %2d", cfgNOK);
      aff(res); ledR(); bipNOK(); delay(1700); ledX();
      break;
    }

    // ── Temps écoulé après buzz → pénalité automatique ─────────
    if (t == 0) {
      J[actif].nok++;
      J[actif].score -= cfgNOK;
      aff("TROP TAR"); ledR(); bipNOK(); delay(1700); ledX();
      break;
    }

    delay(40);
  }

  // ════════════════════════════════════════════════════════════════
  //  PHASE 3 – Score du joueur actif
  // ════════════════════════════════════════════════════════════════
  {
    char sc[5]; fmtSc(sc, J[actif].score);
    char buf[9]; snprintf(buf, 9, "%c   %s", J[actif].lettre, sc);
    aff(buf); delay(1600);
  }

  // ════════════════════════════════════════════════════════════════
  //  PHASE 4 – Défilement de tous les scores
  // ════════════════════════════════════════════════════════════════
  showScores();

  return true;
}

// ══════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ══════════════════════════════════════════════════════════════════
void loop() {

  // ─ 1. Configuration du nombre de joueurs ──────────────────────
  menuNbJoueurs();
  initJ();

  // ─ 2. Boucle de jeu (une question après l'autre) ──────────────
  while (uneQuestion()) {
    // uneQuestion() retourne false quand l'arbitre décide d'arrêter
  }

  // ─ 3. Classement final ────────────────────────────────────────
  showFin();

  // ─ 4. Attente avant relance (appuyer ROK pour rejouer) ────────
  delay(500);
  aff("RESTART "); delay(1500);
  while (!pressed(BTN_OK)) delay(50);
  clr(); delay(300);
}

/*
 * ══════════════════════════════════════════════════════════════════
 *   RÉSUMÉ DES AFFICHAGES 7 SEGMENTS (8 digits)
 * ══════════════════════════════════════════════════════════════════
 *
 *  Écran          │ Affichage    │ Description
 * ────────────────┼──────────────┼─────────────────────────────────
 *  Accueil        │ "  QPC   "   │ Splash screen
 *                 │ "CHAMPION"   │
 *  Cfg joueurs    │ "JOUEUR 4"   │ Nb joueurs sélectionné
 *  Cfg question   │ "TEMPS 15"   │ Temps de question (ex. 15 s)
 *                 │ "OK    10"   │ Points bonne réponse
 *                 │ "NOK    5"   │ Pénalité mauvaise réponse
 *  Attente buzz   │ "----  15"   │ Décompte en cours, tirets = attente
 *                 │ "----    "   │ Clignotement urgence (≤5 s)
 *  Joueur buzzé   │ "A     12"   │ Lettre joueur + temps restant
 *                 │ "A       "   │ Clignotement urgence (≤5 s)
 *  Réponse OK     │ "OK    10"   │ Points gagnés
 *  Réponse NOK    │ "NOK    5"   │ Pénalité appliquée
 *  Timeout        │ "TROP TAR"   │ Temps dépassé après buzz
 *                 │ "ECOULE  "   │ Temps total écoulé sans buzz
 *  Score joueur   │ "A    035"   │ Score courant du joueur actif
 *  Scores         │ "SCORES  "   │ Introduction défilement
 *                 │ "B    020"   │ Score de chaque joueur
 *  Classement     │ "1A   035"   │ Rang + Lettre + Score
 *  Vainqueur      │ "WIN   A "   │ Clignotant
 *  Restart        │ "RESTART "   │ Appuyer ROK pour rejouer
 * ══════════════════════════════════════════════════════════════════
 */
