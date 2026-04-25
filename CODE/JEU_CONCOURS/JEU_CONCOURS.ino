/*
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║         Q U E S T I O N S   P O U R   U N   C H A M P I O N    ║
 * ║         Simulation Arduino UNO + MAX7219 – 8 afficheurs 7-seg   ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  CÂBLAGE MATÉRIEL                                                ║
 * ║  MAX7219   : DIN=11 | CLK=13 | CS/LOAD=10                       ║
 * ║  Buzzers   : Joueur A=7 | B=8 | C=9 | D=12                      ║
 * ║  Boutons   : ROK=2 (valider) | RNOK=3 (invalider)               ║
 * ║              [+]=0 | [-]=1                                       ║
 * ║  LED bic.  : Anode=4 (vert) | Cathode=5 (rouge)                 ║
 * ║  HP/Buzzer : Pin=6 (tone)                                        ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  CORRECTIONS APPORTÉES (v3)                                      ║
 * ║  1. Suppression du goto  →  refactorisé en boucle propre        ║
 * ║  2. Timeout Phase 2 : break manquant → boucle infinie corrigée  ║
 * ║  3. Chrono GELÉ pendant feedbacks (buzz, OK, NOK, affichages)   ║
 * ║     via un accumulateur pauseTotal                               ║
 * ║  4. restant recalculé à chaque itération (plus de valeur figée) ║
 * ║  5. peutBuzzer() simplifiée et correcte (!echoue[i])            ║
 * ║  6. Score joueur pénalisé affiché brièvement avant de reprendre ║
 * ║  7. Clignotement urgence (≤5 s) remis en Phase 1 et Phase 2     ║
 * ║  8. showScores() appelé dans tous les cas de fin de question    ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#include <LedControl.h>

// ══════════════════════════════════════════════════════════════════
//  BROCHES
// ══════════════════════════════════════════════════════════════════
#define DIN_PIN 11
#define CLK_PIN 13
#define LOAD_PIN 10

#define BTN_OK 2     // Valider (bonne réponse / confirmer menu)
#define BTN_NOK 3    // Invalider (mauvaise réponse)
#define BTN_PLUS 0   // [+] — incrémenter / basculer FIN
#define BTN_MOINS 1  // [-] — décrémenter / basculer FIN

#define BJ_A 7
#define BJ_B 8
#define BJ_C 9
#define BJ_D 12

#define BUZZER 6
#define LED_A 4  // Anode  → vert quand HIGH
#define LED_C 5  // Cathode → rouge quand HIGH

// ══════════════════════════════════════════════════════════════════
//  MAX7219
// ══════════════════════════════════════════════════════════════════
LedControl lc = LedControl(DIN_PIN, CLK_PIN, LOAD_PIN, 1);

// ══════════════════════════════════════════════════════════════════
//  STRUCTURE JOUEUR
// ══════════════════════════════════════════════════════════════════
struct Joueur {
  char lettre;
  int score;
  int ok;
  int nok;
};

// ══════════════════════════════════════════════════════════════════
//  VARIABLES GLOBALES
// ══════════════════════════════════════════════════════════════════
Joueur J[4];
int nbJ = 4;
int actif = -1;

const int BJ_PINS[4] = { BJ_A, BJ_B, BJ_C, BJ_D };

int cfgT = 15;   // Temps (5–60 s, pas 5)
int cfgOK = 10;  // Points bonne réponse (1–20)
int cfgNOK = 5;  // Pénalité mauvaise réponse (0–10)

// Mode Flash : true = une mauvaise réponse ferme la question immédiatement
// (les autres joueurs ne peuvent PAS reprendre après un NOK)
bool modeFlash = false;

// ══════════════════════════════════════════════════════════════════
//  AFFICHAGE 7 SEGMENTS
// ══════════════════════════════════════════════════════════════════
byte seg7(char c) {
  switch (toupper(c)) {
    case 'A': return 0x77;
    case 'B': return 0x1F;
    case 'C': return 0x4E;
    case 'D': return 0x3D;
    case 'E': return 0x4F;
    case 'F': return 0x47;
    case 'G': return 0x5E;
    case 'H': return 0x37;
    case 'I': return 0x30;
    case 'J': return 0x3C;
    case 'K': return 0x37;
    case 'L': return 0x0E;
    case 'M': return 0x76;
    case 'N': return 0x17;
    case 'O': return 0x7E;
    case 'P': return 0x67;
    case 'Q': return 0x73;
    case 'R': return 0x05;
    case 'S': return 0x5B;
    case 'T': return 0x0F;
    case 'U': return 0x3E;
    case 'Y': return 0x3B;
    case 'Z': return 0x6D;
    case '0': return 0x7E;
    case '1': return 0x30;
    case '2': return 0x6D;
    case '3': return 0x79;
    case '4': return 0x33;
    case '5': return 0x5B;
    case '6': return 0x5F;
    case '7': return 0x70;
    case '8': return 0x7F;
    case '9': return 0x7B;
    case '-': return 0x01;
    case '_': return 0x08;
    case '?': return 0x65;
    case ' ': return 0x00;
    default: return 0x00;
  }
}

void clr() {
  lc.clearDisplay(0);
}

void aff(const char* s) {
  for (int i = 0; i < 8; i++)
    lc.setRow(0, i, (s[i] != '\0') ? seg7(s[i]) : 0x00);
}

/* Score signé sur 4 chars : " 035" ou "-005" */
void fmtSc(char* out, int v) {
  int av = (v < 0) ? -v : v;
  if (av > 999) av = 999;
  snprintf(out, 5, "%c%03d", (v < 0 ? '-' : ' '), av);
}

// ══════════════════════════════════════════════════════════════════
//  SON
// ══════════════════════════════════════════════════════════════════
void note(int f, int ms) {
  tone(BUZZER, f, ms);
  delay(ms + 25);
}

void bipMenu() {
  note(700, 60);
}
void bipBuzz() {
  note(1400, 70);
  delay(40);
  note(1400, 70);
}  // ~230 ms
void bipOK() {
  note(880, 110);
  note(1100, 190);
}  // ~350 ms
void bipNOK() {
  note(350, 380);
}  // ~405 ms
void bipTemps() {
  note(400, 200);
  note(300, 350);
}
void bipFin() {
  note(1000, 100);
  note(1200, 100);
  note(1500, 250);
}
void bipStart() {
  note(440, 80);
  note(660, 80);
  note(880, 160);
}

// ══════════════════════════════════════════════════════════════════
//  LED BICOLORE — COULEURS ET ANIMATIONS
//
//  Câblage réel : RGB avec bleu à la masse
//    • LED_A = pin 4  → vert  (NON-PWM → soft-PWM logiciel)
//    • LED_C = pin 5  → rouge (PWM matériel via analogWrite)
//
//  Principe du soft-PWM vert :
//    Pour chaque "pixel-time" de SPWM_US µs, on allume la broche
//    pendant (g * SPWM_US / 255) µs puis on l'éteint le reste.
//    Fréquence effective ≈ 1 / SPWM_US = ~2 kHz  (invisible à l'œil)
//
//  Mélange de couleurs (r=0-255, g=0-255) :
//    r=255 g=0   → rouge pur
//    r=0   g=255 → vert pur
//    r=255 g=255 → jaune
//    r=255 g=80  → orange
//    r=0   g=0   → éteinte
// ══════════════════════════════════════════════════════════════════

#define SPWM_US 500  // Période soft-PWM vert en µs (~2 kHz)

// ── Primitives de base ───────────────────────────────────────────

/** Vert pur (digital rapide pour les feedbacks temps-réel) */
void ledV() {
  analogWrite(LED_C, 0);
  digitalWrite(LED_A, HIGH);
}
/** Rouge pur */
void ledR() {
  analogWrite(LED_C, 255);
  digitalWrite(LED_A, LOW);
}
/** Éteinte — remet les deux broches à 0 */
void ledX() {
  analogWrite(LED_C, 0);
  digitalWrite(LED_A, LOW);
}

// ── Couleur mixée ────────────────────────────────────────────────

/**
 * Applique une couleur (r, g) pendant UNE période de soft-PWM.
 * À appeler en boucle serrée pour maintenir la couleur dans le temps.
 *
 * r : 0–255  intensité rouge  (analogWrite → pin 5, PWM matériel)
 * g : 0–255  intensité vert   (soft-PWM    → pin 4, logiciel)
 */
void setColor(byte r, byte g) {
  analogWrite(LED_C, r);  // rouge : PWM matériel, sans blocage

  // vert : une impulsion proportionnelle sur SPWM_US µs
  if (g == 0) {
    digitalWrite(LED_A, LOW);
    delayMicroseconds(SPWM_US);
  } else if (g == 255) {
    digitalWrite(LED_A, HIGH);
    delayMicroseconds(SPWM_US);
  } else {
    unsigned int tOn = (unsigned int)g * SPWM_US / 255;
    digitalWrite(LED_A, HIGH);
    delayMicroseconds(tOn);
    digitalWrite(LED_A, LOW);
    delayMicroseconds(SPWM_US - tOn);
  }
}

/**
 * Maintient la couleur (r, g) pendant `duree_ms` ms
 * en bouclant sur setColor() → le soft-PWM tourne en continu.
 */
void holdColor(byte r, byte g, unsigned int duree_ms) {
  unsigned long fin = millis() + duree_ms;
  while ((long)(millis() - fin) < 0) setColor(r, g);
  analogWrite(LED_C, 0);
  digitalWrite(LED_A, LOW);
}

// ── Animations ───────────────────────────────────────────────────

/**
 * Fondu entrant (0→max) puis sortant (max→0) en `duree_ms` ms.
 * rMax, gMax : couleur cible au sommet du pulse.
 * steps      : nombre de paliers (granularité, 32–64 conseillé).
 */
void ledPulse(byte rMax, byte gMax, unsigned int duree_ms,
              int steps = 48) {
  int stepMs = (int)(duree_ms / (2 * steps));
  if (stepMs < 1) stepMs = 1;

  for (int i = 0; i <= steps; i++) {
    holdColor((byte)((long)rMax * i / steps),
              (byte)((long)gMax * i / steps),
              stepMs);
  }
  for (int i = steps; i >= 0; i--) {
    holdColor((byte)((long)rMax * i / steps),
              (byte)((long)gMax * i / steps),
              stepMs);
  }
}

/** Pulse vert — validation bonne réponse */
void ledPulseVert() {
  ledPulse(0, 255, 700);
}

/** Pulse rouge — pénalité mauvaise réponse */
void ledPulseRouge() {
  ledPulse(255, 0, 700);
}

/** Pulse orange — tension / urgence */
void ledPulseOrange(int n = 2) {
  for (int i = 0; i < n; i++) ledPulse(255, 80, 500);
}

/**
 * Dégradé rouge → orange → jaune → vert en `duree_ms` ms.
 * Utile pour une transition "bonne réponse surprise".
 */
void ledArcEnCiel(unsigned int duree_ms = 600) {
  int steps = 40;
  int stepMs = (int)(duree_ms / (2 * steps));
  if (stepMs < 1) stepMs = 1;

  // Rouge → Jaune (g monte de 0 à 255, r reste à 255)
  for (int i = 0; i <= steps; i++)
    holdColor(255, (byte)(255 * i / steps), stepMs);

  // Jaune → Vert (r descend de 255 à 0, g reste à 255)
  for (int i = steps; i >= 0; i--)
    holdColor((byte)(255 * i / steps), 255, stepMs);
}

/**
 * Clignotement alterné rouge / vert — n fois, période en ms.
 * Ex. ledAlterne(4, 200) = 4 cycles de 200 ms chacun.
 */
void ledAlterne(int n = 4, unsigned int periodeMs = 200) {
  for (int i = 0; i < n; i++) {
    holdColor(255, 0, periodeMs / 2);  // rouge
    ledX();
    delay(20);
    holdColor(0, 255, periodeMs / 2);  // vert
    ledX();
    delay(20);
  }
}

/**
 * Respiration lente orange-rouge — effet tension pendant le chrono.
 * `cycles` : nombre de respirations complètes.
 */
void ledRespiration(int cycles = 3) {
  for (int c = 0; c < cycles; c++) {
    for (int i = 0; i <= 48; i++)
      holdColor((byte)(255 * i / 48), (byte)(60 * i / 48), 12);
    for (int i = 48; i >= 0; i--)
      holdColor((byte)(255 * i / 48), (byte)(60 * i / 48), 12);
  }
}

/**
 * Animation vainqueur : 3 arcs en ciel + 2 pulses verts finaux.
 * Appelée dans showFin() pour le 1er du classement.
 */
void ledVainqueur() {
  for (int i = 0; i < 3; i++) ledArcEnCiel(400);
  ledPulseVert();
  ledPulseVert();
  ledX();
}

// ══════════════════════════════════════════════════════════════════
//  ANTI-REBOND (60 ms)
//  Les tableaux sont indexés par numéro de broche (max pin = 13).
// ══════════════════════════════════════════════════════════════════
static unsigned long tDb[14] = { 0 };
static bool bPrev[14] = { false };

bool pressed(int pin) {
  bool cur = (digitalRead(pin) == LOW);
  if (cur && !bPrev[pin] && (millis() - tDb[pin] > 60)) {
    tDb[pin] = millis();
    bPrev[pin] = true;
    return true;
  }
  if (!cur) bPrev[pin] = false;
  return false;
}

// ══════════════════════════════════════════════════════════════════
//  INIT JOUEURS
// ══════════════════════════════════════════════════════════════════
void initJ() {
  const char L[4] = { 'A', 'B', 'C', 'D' };
  for (int i = 0; i < 4; i++) {
    J[i].lettre = L[i];
    J[i].score = 0;
    J[i].ok = 0;
    J[i].nok = 0;
  }
}

// ══════════════════════════════════════════════════════════════════
//  HELPER : Calcule le temps restant en tenant compte des pauses
// ══════════════════════════════════════════════════════════════════
// tStart      : millis() au début de la question
// pauseTotal  : accumulateur des durées gelées (feedbacks, sons...)
// Retourne le nombre de secondes restantes (≥ 0)
int tempsRestant(unsigned long tStart, unsigned long pauseTotal) {
  long ecoule = (long)((millis() - tStart - pauseTotal) / 1000UL);
  int r = (int)(cfgT - ecoule);
  return (r < 0) ? 0 : r;
}

// ══════════════════════════════════════════════════════════════════
//  HELPER : Pause chrono — exécute une action bloquante et
//           ajoute sa durée réelle à pauseTotal
//  Usage :  pauseTotal += pauseAction([lambda-like code]);
//  → On appelle directement avant/après chaque bloc bloquant.
// ══════════════════════════════════════════════════════════════════
// (pas de lambda en C++11 Arduino → on inline les pauses manuellement)

// ══════════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════════
void setup() {
  int inp[8] = { BTN_OK, BTN_NOK, BTN_PLUS, BTN_MOINS,
                 BJ_A, BJ_B, BJ_C, BJ_D };
  for (int i = 0; i < 8; i++) pinMode(inp[i], INPUT_PULLUP);

  pinMode(BUZZER, OUTPUT);
  pinMode(LED_A, OUTPUT);
  pinMode(LED_C, OUTPUT);
  ledX();

  lc.shutdown(0, false);
  lc.setIntensity(0, 12);
  clr();
  initJ();

  aff(" STIC24 ");
  ledArcEnCiel(900);
  delay(100);
  aff("  QUIZ  ");
  ledPulseOrange(2);
  delay(100);
  aff(" CONFIG ");
  delay(600);
  clr();
  delay(300);
}

// ══════════════════════════════════════════════════════════════════
//  MENU – Nombre de joueurs
// ══════════════════════════════════════════════════════════════════
void menuNbJoueurs() {
  while (true) {
    char buf[9];
    snprintf(buf, 9, "JOUEUR %1d", nbJ);
    aff(buf);
    if (pressed(BTN_PLUS)) {
      nbJ++;
      if (nbJ > 4) nbJ = 2;
      bipMenu();
    }
    if (pressed(BTN_MOINS)) {
      nbJ--;
      if (nbJ < 2) nbJ = 4;
      bipMenu();
    }
    if (pressed(BTN_OK)) {
      bipMenu();
      break;
    }
    delay(20);
  }
}

// ══════════════════════════════════════════════════════════════════
//  MENU – Mode de jeu : FLASH ou NORMAL
//  Affiché une seule fois après la sélection du nombre de joueurs.
//  [+] ou [-]  →  bascule NORMAL ↔ FLASH
//  ROK         →  confirme
//
//  NORMAL : après un NOK, les autres joueurs peuvent encore buzzer
//  FLASH  : un seul NOK ferme la question immédiatement
// ══════════════════════════════════════════════════════════════════
void menuModeFlash() {
  while (true) {
    aff(modeFlash ? "MD FLASH" : "MD NORMA ");
    if (pressed(BTN_PLUS) || pressed(BTN_MOINS)) {
      modeFlash = !modeFlash;
      bipMenu();
    }
    if (pressed(BTN_OK)) {
      bipMenu();
      break;
    }
    delay(20);
  }
  // Confirmation visuelle du mode choisi (1 s)
  aff(modeFlash ? "FLASH ON" : "NORMA ON");
  delay(900);
}

// ══════════════════════════════════════════════════════════════════
//  MENU – FIN DE PARTIE ?
//  [+] ou [-]  →  bascule NON ↔ OUI
//  ROK         →  confirme
//  Retourne true  = l'arbitre veut arrêter
//  Retourne false = continuer
// ══════════════════════════════════════════════════════════════════
bool menuFin() {
  bool fin = false;
  while (true) {
    aff(fin ? "FIN? OUI" : "FIN? NON");
    if (pressed(BTN_PLUS) || pressed(BTN_MOINS)) {
      fin = !fin;
      bipMenu();
    }
    if (pressed(BTN_OK)) {
      bipMenu();
      // Si OUI → demander confirmation avec SUR?
      if (fin) {
        bool sure = false;
        while (true) {
          char buf[9];
          snprintf(buf, 9, "SUR? %s", sure ? "OUI" : "NON");
          aff(buf);
          if (pressed(BTN_PLUS) || pressed(BTN_MOINS)) {
            sure = !sure;
            bipMenu();
          }
          if (pressed(BTN_OK)) {
            bipMenu();
            break;
          }
          delay(20);
        }
        return sure;  // true = vraiment fini | false = revenir au menu FIN
      }
      return false;  // NON confirmé → continuer
    }
    delay(20);
  }
}

// ══════════════════════════════════════════════════════════════════
//  MENU – Configuration de la question
//  Étapes : TEMPS → OK → NOK → GO
// ══════════════════════════════════════════════════════════════════
void menuQuestion() {
  while (true) {
    char buf[9];
    snprintf(buf, 9, "TEMPS %2d", cfgT);
    aff(buf);
    if (pressed(BTN_PLUS)) {
      cfgT += 5;
      if (cfgT > 60) cfgT = 5;
      bipMenu();
    }
    if (pressed(BTN_MOINS)) {
      cfgT -= 5;
      if (cfgT < 5) cfgT = 60;
      bipMenu();
    }
    if (pressed(BTN_OK)) {
      bipMenu();
      break;
    }
    delay(20);
  }
  while (true) {
    char buf[9];
    snprintf(buf, 9, "OK    %2d", cfgOK);
    aff(buf);
    if (pressed(BTN_PLUS)) {
      cfgOK++;
      if (cfgOK > 20) cfgOK = 1;
      bipMenu();
    }
    if (pressed(BTN_MOINS)) {
      cfgOK--;
      if (cfgOK < 1) cfgOK = 20;
      bipMenu();
    }
    if (pressed(BTN_OK)) {
      bipMenu();
      break;
    }
    delay(20);
  }
  while (true) {
    char buf[9];
    snprintf(buf, 9, "NOK   %2d", cfgNOK);
    aff(buf);
    if (pressed(BTN_PLUS)) {
      cfgNOK++;
      if (cfgNOK > 10) cfgNOK = 0;
      bipMenu();
    }
    if (pressed(BTN_MOINS)) {
      cfgNOK--;
      if (cfgNOK < 0) cfgNOK = 10;
      bipMenu();
    }
    if (pressed(BTN_OK)) {
      bipMenu();
      break;
    }
    delay(20);
  }
  aff("  GO    ");
  bipStart();
  delay(700);
}

// ══════════════════════════════════════════════════════════════════
//  SCORES INTERMÉDIAIRES
// ══════════════════════════════════════════════════════════════════
void showScores() {
  aff("SCORES  ");
  delay(700);
  for (int i = 0; i < nbJ; i++) {
    char sc[5];
    fmtSc(sc, J[i].score);
    char buf[9];
    snprintf(buf, 9, "%c   %s", J[i].lettre, sc);
    aff(buf);
    delay(1400);
  }
}

// ══════════════════════════════════════════════════════════════════
//  CLASSEMENT FINAL
// ══════════════════════════════════════════════════════════════════
void showFin() {
  bipFin();
  aff("FIN JEU ");
  delay(1500);

  Joueur tri[4];
  for (int i = 0; i < nbJ; i++) tri[i] = J[i];
  for (int i = 0; i < nbJ - 1; i++)
    for (int k = 0; k < nbJ - i - 1; k++)
      if (tri[k].score < tri[k + 1].score) {
        Joueur tmp = tri[k];
        tri[k] = tri[k + 1];
        tri[k + 1] = tmp;
      }

  aff("CLASSMT ");
  delay(1000);
  for (int i = 0; i < nbJ; i++) {
    char sc[5];
    fmtSc(sc, tri[i].score);
    char buf[9];
    snprintf(buf, 9, "%d%c  %s", i + 1, tri[i].lettre, sc);
    aff(buf);
    delay(2500);
  }

  char win[9];
  snprintf(win, 9, "WIN   %c ", tri[0].lettre);
  // Clignotement alterné rouge/vert × 3 avant l'annonce du vainqueur
  ledAlterne(3, 220);
  for (int i = 0; i < 7; i++) {
    (i % 2 == 0) ? aff(win) : clr();
    if (i % 2 == 0) bipOK();
    delay(380);
  }
  aff(win);
  // Animation finale vainqueur : 3 arcs en ciel + 2 pulses verts
  ledVainqueur();
}

// ══════════════════════════════════════════════════════════════════
//  CŒUR DU JEU :  Traitement d'UNE question
//
//  ┌─ NOUVEAU COMPORTEMENT ────────────────────────────────────────┐
//  │  • Le CHRONO EST GELÉ pendant tous les feedbacks :           │
//  │    bipBuzz, affichage OK/NOK, délais, sons → pauseTotal      │
//  │  • Après un NOK, le joueur est BLOQUÉ pour cette question    │
//  │    mais les autres peuvent encore buzzer avec le TEMPS        │
//  │    RESTANT (chrono qui reprend là où il s'était arrêté)      │
//  │  • La question se ferme si :                                  │
//  │      – Bonne réponse (ROK)                                   │
//  │      – Tous les joueurs sont bloqués                         │
//  │      – Temps total écoulé                                    │
//  │  • Pas de goto, pas d'overflow mémoire                       │
//  └───────────────────────────────────────────────────────────────┘
//
//  Retourne true  → continuer le jeu
//  Retourne false → menuFin() a retourné true (arrêt demandé)
// ══════════════════════════════════════════════════════════════════
bool uneQuestion() {

  // ── Étape 0 : FIN ? ──────────────────────────────────────────
  if (menuFin()) return false;

  // ── Étape 1 : Config TEMPS / OK / NOK ────────────────────────
  menuQuestion();

  // ── État de la question ───────────────────────────────────────
  bool echoue[4] = { false, false, false, false };
  unsigned long tStart = millis();
  unsigned long pauseTotal = 0;  // ← accumulateur de temps gelé

  // ════════════════════════════════════════════════════════════
  //  BOUCLE PRINCIPALE (une tentative par itération)
  //  Phase 1 : attente du buzz
  //  Phase 2 : joueur actif, attente décision arbitre
  // ════════════════════════════════════════════════════════════
  while (true) {

    // ── Calculer les joueurs encore disponibles ───────────────
    int nbLibres = 0;
    for (int i = 0; i < nbJ; i++)
      if (!echoue[i]) nbLibres++;

    // ── Conditions d'arrêt de la question ────────────────────
    int r = tempsRestant(tStart, pauseTotal);

    if (r <= 0) {
      aff("ECOULE  ");
      bipTemps();
      delay(1300);
      showScores();
      return true;
    }
    if (nbLibres == 0) {
      aff("PLUS NOK");
      bipTemps();
      delay(1200);
      showScores();
      return true;
    }

    // ══════════════════════════════════════════════════════════
    //  PHASE 1 — Attente du buzz
    //  Seuls les joueurs non bloqués sont surveillés.
    //  Clignotement urgence sous 5 s restantes.
    // ══════════════════════════════════════════════════════════
    actif = -1;

    while (actif < 0) {
      r = tempsRestant(tStart, pauseTotal);

      if (r <= 0) {
        aff("ECOULE  ");
        bipTemps();
        delay(1300);
        showScores();
        return true;
      }

      // Affichage "----  15" avec clignotement urgence
      char buf[9];
      bool clig = (r <= 5) && ((millis() / 400) % 2 == 0);
      snprintf(buf, 9, clig ? "----    " : "----  %2d", r);
      aff(buf);

      // Scan buzzers (joueurs non bloqués uniquement)
      for (int i = 0; i < nbJ; i++) {
        if (!echoue[i] && digitalRead(BJ_PINS[i]) == LOW) {
          actif = i;
          break;
        }
      }

      delay(40);
    }

    // ══════════════════════════════════════════════════════════
    //  PHASE 2 — Joueur actif, arbitre tranche
    //
    //  ⚠ Le chrono est GELÉ dès le bipBuzz jusqu'à la fin
    //    du feedback (son + affichage + délai).
    //    → pauseTotal absorbe tout le temps bloquant.
    // ══════════════════════════════════════════════════════════

    // — Gel : son + clignotement orange au buzz —
    {
      unsigned long p = millis();
      bipBuzz();
      ledPulseOrange(1);  // 1 pulse orange = tension, alerte visuelle (~500 ms)
      pauseTotal += millis() - p;
    }

    // Temps restant au moment où le joueur a la parole
    int tAuBuzz = tempsRestant(tStart, pauseTotal);

    // Référence temporelle de CETTE prise de parole (hors pauses globales)
    unsigned long tParole = millis();
    unsigned long pauseParole = 0;  // pauses propres à cette phase 2

    while (true) {

      // Temps restant pour CE joueur (son compteur personnel)
      int t = tAuBuzz - (int)((millis() - tParole - pauseParole) / 1000UL);
      if (t < 0) t = 0;

      // Affichage "A     12" avec clignotement urgence
      char buf[9];
      bool clig = (t <= 5) && ((millis() / 300) % 2 == 0);
      snprintf(buf, 9, clig ? "%c       " : "%c     %2d", J[actif].lettre, t);
      aff(buf);

      // ── ROK : Bonne réponse → ferme la question ─────────────
      if (pressed(BTN_OK)) {
        J[actif].ok++;
        J[actif].score += cfgOK;

        // — Gel : feedback OK —
        unsigned long p = millis();
        char res[9];
        snprintf(res, 9, "OK    %2d", cfgOK);
        aff(res);
        bipOK();         // son (~350 ms)
        ledPulseVert();  // fondu vert entrant/sortant (700 ms)
        pauseTotal += millis() - p;

        // Score du joueur
        char sc[5];
        fmtSc(sc, J[actif].score);
        char sc2[9];
        snprintf(sc2, 9, "%c   %s", J[actif].lettre, sc);
        aff(sc2);
        delay(1400);

        showScores();
        return true;
      }

      // ── RNOK ou timeout : Mauvaise réponse → joueur bloqué ──
      bool nok_btn = pressed(BTN_NOK);
      bool nok_timeout = (t == 0);

      if (nok_btn || nok_timeout) {
        J[actif].nok++;
        J[actif].score -= cfgNOK;
        echoue[actif] = true;  // ← bloqué pour cette question

        // — Gel : feedback NOK —
        unsigned long p = millis();
        char res[9];
        snprintf(res, 9, nok_timeout ? "TROP TAR" : "NOK   %2d", cfgNOK);
        aff(res);
        bipNOK();         // son grave (~405 ms)
        ledPulseRouge();  // fondu rouge entrant/sortant (700 ms)
        pauseTotal += millis() - p;

        // Score pénalisé (bref)
        char sc[5];
        fmtSc(sc, J[actif].score);
        char sc2[9];
        snprintf(sc2, 9, "%c   %s", J[actif].lettre, sc);
        p = millis();
        aff(sc2);
        delay(900);
        pauseTotal += millis() - p;

        actif = -1;

        // MODE FLASH : une mauvaise réponse ferme la question
        if (modeFlash) {
          showScores();
          return true;
        }

        break;  // ← sort de Phase 2, retour en haut de la boucle principale
                //   Le chrono repart là où il était (pauseTotal à jour)
      }

      delay(40);
    }
    // ↑ Retour en haut de la boucle principale → Phase 1 avec chrono intact
  }
}

// ══════════════════════════════════════════════════════════════════
//  LOOP PRINCIPAL
// ══════════════════════════════════════════════════════════════════
void loop() {
  menuNbJoueurs();
  menuModeFlash();  // ← choix du mode juste après le nombre de joueurs
  initJ();

  while (uneQuestion()) { /* continuer tant que menuFin() → NON */
  }

  showFin();

  delay(500);
  aff("RESTART ");
  delay(1500);
  while (!pressed(BTN_OK)) delay(50);
  clr();
  delay(300);
}

/*
 * ══════════════════════════════════════════════════════════════════
 *   RÉCAPITULATIF DES AFFICHAGES 7 SEGMENTS
 * ══════════════════════════════════════════════════════════════════
 *
 *  Affichage     Description
 * ──────────────────────────────────────────────────────────────────
 *  " STIC24 "    Splash screen
 *  "  QUIZ  "    Splash screen
 *  " CONFIG "    Splash screen
 *  "JOUEUR 4"    Menu nombre de joueurs
 *  "FL NRML "    Menu mode — Normal (autres joueurs peuvent rebuzzer)
 *  "FL FLASH"    Menu mode — Flash ([+]/[-] bascule, ROK confirme)
 *  "NRML  ON"    Confirmation mode Normal
 *  "FLASH ON"    Confirmation mode Flash
 *  "FIN? NON"    Menu fin — option courante NON
 *  "FIN? OUI"    Menu fin — option courante OUI  ([+]/[-] bascule)
 *  "SUR?  NON"   Confirmation avant fin de partie
 *  "SUR?  OUI"   Confirmation avant fin de partie
 *  "TEMPS 15"    Config : durée du chrono
 *  "OK    10"    Config : points bonne réponse
 *  "NOK    5"    Config : pénalité
 *  "  GO    "    Lancement de la question
 *  "----  15"    Phase 1 : chrono visible (chrono gelé pendant feedbacks)
 *  "----    "    Phase 1 : clignotement urgence (≤ 5 s)
 *  "A     12"    Phase 2 : lettre joueur + temps restant
 *  "A       "    Phase 2 : clignotement urgence (≤ 5 s)
 *  "OK    10"    Bonne réponse → fin de question
 *  "NOK    5"    Mauvaise réponse → joueur bloqué, autres peuvent buzzer
 *  "TROP TAR"    Timeout après buzz → pénalité automatique
 *  "A    035"    Score du joueur après sa réponse
 *  "ECOULE  "    Temps total écoulé (sans bonne réponse)
 *  "PLUS NOK"    Tous les joueurs bloqués → fin de question
 *  "SCORES  "    Intro défilement des scores
 *  "B    020"    Score de chaque joueur en séquence
 *  "1A   035"    Classement final — rang + lettre + score
 *  "WIN   A "    Vainqueur (clignotant × 7)
 *  "RESTART "    Appuyer ROK pour rejouer
 * ══════════════════════════════════════════════════════════════════
 */
