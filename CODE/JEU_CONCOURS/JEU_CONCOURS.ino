#include <LedControl.h>

// ============================================================
//  CONFIGURATION MATÉRIELLE (HARDWARE CONFIG)
// ============================================================
#define DIN_PIN 11
#define CLK_PIN 13
#define LOAD_PIN 10
#define NB_AFFICH 1
#define RNOK 3
#define ROK 2
#define MODE 1
#define R0 0
#define ANO 4
#define CAT 5
#define HP 6
#define BJ1 7
#define BJ2 8
#define BJ3 9
#define BJ4 12
//leds
LedControl lc = LedControl(DIN_PIN, CLK_PIN, LOAD_PIN, NB_AFFICH);

// --- VARIABLES TEMPS (CORRIGÉES) ---
unsigned long temps_derniere_action = 0; 
unsigned long maintenant = 0;

// --- TABLEAU D'OBJETS JOUEURS ---

int TEMPS_QUESTION = 15; 
int tempsQuestion = TEMPS_QUESTION;


//VARIABLES joueurs
typedef struct  {
  String nom;          // Nom du joueur (ex: "J1")
  int nbOK;            // Réponses justes
  int nbNOK;           // Réponses fausses
  int nbZero;          // Réponses nulles (ou non répondues)
  long score;          // Score total
}Joueur;
int nJoueurs = 4;
Joueur joueurs[4];

// ============================================================
//  PROTOTYPES DES FONCTIONS (MODULARITÉ)
// ============================================================
void initialiserAffichage();
void configurerComposants();
void configurerPartie();
void afficherChaine(const char* texte);
void effacerEcran();
byte charTo7Seg(char c);
String obtenirTempsString();

void afficherTemps();

void aBuzze(int j);
void tempsEcoule();
void verifierReponse();
void initialiserDonneesJoueurs() {
  for (int i = 0; i < 4; i++) {
    joueurs[i].nom = "J" + String(i + 1);
    joueurs[i].nbOK = 0;
    joueurs[i].nbNOK = 0;
    joueurs[i].nbZero = 0;
    joueurs[i].score = 0;
  }
}
// ============================================================
//  FONCTION PRINCIPALE : SETUP
// ============================================================
void setup() {
  configurerComposants();
  initialiserAffichage();
  
  // Test de la modularité
  afficherChaine("STIC 24");
  delay(1500);
  effacerEcran();
  afficherChaine("JEU QUI2");  // Message de ton schéma
  delay(1500);
  configurerPartie();
  effacerEcran();
  temps_derniere_action = millis();
}

// ============================================================
//  FONCTION PRINCIPALE : LOOP
// ============================================================
void loop() {
  // Le loop reste vide pour l'instant, prêt pour la logique de jeu

  afficherTemps();
  verifierReponse();
}

// ============================================================
//  COUCHE 1 : GESTION BAS NIVEAU (SEGMENTS)
// ============================================================

/**
 * Convertit un caractère ASCII en code 7-segments.
 * Facile à mettre à jour si tu veux ajouter des symboles.
 */
byte charTo7Seg(char c) {
  switch (toupper(c)) {
    // Lettres
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
    case 'L': return 0x0E;
    case 'O': return 0x7E;
    case 'P': return 0x67;
    case 'R': return 0x05;
    case 'S': return 0x5B;
    case 'T': return 0x0F;
    case 'U': return 0x3E;
    // caractères vraiment spéciaux
    case 'M': return 0x76;
    case 'N': return 0x25;
    case 'Q': return 0x73;
    case 'Y': return 0x3B;
    // Chiffres
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
    // Symboles
    case '-': return 0x01;
    case '?': return 0x6B;
    case '_': return 0x08;
    case ' ': return 0x00;
    default: return 0x00;  // Caractère inconnu = éteint
  }
}

// ============================================================
//  COUCHE 2 : INTERFACE D'AFFICHAGE (LOGIC)
// ============================================================

/**
 * Configure le MAX7219 au démarrage.
 */
void initialiserAffichage() {
  lc.shutdown(0, false);  // Réveil
  lc.setIntensity(0, 8);  // Luminosité moyenne
  lc.clearDisplay(0);     // Nettoyage
}

/**
 * Affiche une chaîne de caractères (max 8) de gauche à droite.
 * Gère l'inversion observée sur ton Proteus.
 */
void afficherChaine(const char* texte) {
  effacerEcran();
  int n = strlen(texte);

  for (int i = 0; i < n && i < 8; i++) {
    // On utilise l'index 'i' car ton afficheur Proteus
    // semble mapper DIG0 à la position la plus à gauche.
    lc.setRow(0, i, charTo7Seg(texte[i]));
  }
}

/**
 * Éteint tous les segments de l'afficheur.
 */
void effacerEcran() {
  lc.clearDisplay(0);
}

/**
 * Retourne le temps écoulé depuis le démarrage.
 * Format : "MM-SS" (ex: "01-25" pour 1min 25s)
 */
String obtenirTempsString() {
  
  tempsQuestion = TEMPS_QUESTION - (millis() - temps_derniere_action)/1000;
  int minutes = tempsQuestion/ 60;
  int secondes = tempsQuestion % 60;
  

  String tempsFormate = "";

  // Ajout du zéro initial pour les minutes si < 10
  if (minutes < 10) tempsFormate += "0";
  tempsFormate += String(minutes);

  tempsFormate += "-";  // Séparateur

  // Ajout du zéro initial pour les secondes si < 10
  if (secondes < 10) tempsFormate += "0";
  tempsFormate += String(secondes);

  return tempsFormate;
}

void afficherTemps() {
  String tempsActuel = obtenirTempsString();

  // On convertit la String en tableau de caractères (char*)

  // pour notre fonction afficherChaine

  afficherChaine(tempsActuel.c_str());
  }
  
  // Ici, il n'y a PAS de delay. 
  // L'Arduino sort de la fonction instantanément si les 500ms ne sont pas passées.

void aBuzze(int j) {
  String af = "BU22 " + joueurs[j].nom;
  afficherChaine(af.c_str());delay(200);
  analogWrite(HP,200);
  delay(100);
  analogWrite(HP,0);
  temps_derniere_action = millis();
}
void tempsEcoule() {
  afficherChaine("ecoule");
  delay(1000);
  temps_derniere_action = millis();
}
void validerReponse(int id) {
  bool attenteArbitre = true;
  afficherChaine("JUSTE ?"); // On demande à l'arbitre de trancher

  while (attenteArbitre) {
    // Si l'arbitre valide (Bouton ROK)
    if (digitalRead(ROK) == LOW) {
      joueurs[id].nbOK++;
      joueurs[id].score += 10;
      afficherChaine("CORRECT");
      digitalWrite(ANO,HIGH);
      digitalWrite(CAT,LOW);
      attenteArbitre = false;
      delay(1000);
    } 
    // Si l'arbitre refuse (Bouton RNOK)
    else if (digitalRead(RNOK) == LOW) {
      joueurs[id].nbNOK++;
      joueurs[id].score -= 5;
      afficherChaine("DOMMAGE");
      digitalWrite(ANO,LOW);
      digitalWrite(CAT,HIGH);
      attenteArbitre = false;
      delay(1000);
    }
    digitalWrite(ANO,LOW);
    digitalWrite(CAT,LOW);
  }
  temps_derniere_action = millis(); // On relance le chrono pour la suite
}
void verifierReponse() {
  int pinsBuzzers[] = {BJ1, BJ2, BJ3, BJ4};

  // On scanne uniquement le nombre de joueurs configurés
  for (int i = 0; i < nJoueurs; i++) {
    if (digitalRead(pinsBuzzers[i]) == LOW) {
      aBuzze(i);           // Son + Message "BUZZ"
      validerReponse(i);  // Mise à jour de l'objet joueur[i]
      return; 
    }
  }

  // Si le temps est mort
  if (tempsQuestion <= 0) {
    tempsEcoule();
    // On pourrait pénaliser tout le monde ici (nbZero++)
    for(int i=0; i<nJoueurs; i++) joueurs[i].nbZero++;
  }
}
  

void configurerComposants() {
  // Tous les boutons en entrées avec résistance interne
  int inputPins[] = {BJ1, BJ2, BJ3, BJ4, ROK, RNOK, MODE, R0};
  for (int i = 0; i < 8; i++) {
    pinMode(inputPins[i], INPUT_PULLUP);
  }

  pinMode(HP, OUTPUT);
  pinMode(ANO, OUTPUT);
  pinMode(CAT, OUTPUT);
}
  

void configurerPartie()
{
  while(digitalRead(ROK)){
   String af = "NOMBRE "+ String(nJoueurs);
  afficherChaine(af.c_str());

    
    if (!digitalRead(MODE))
    {
      delay(200);
      nJoueurs --;
      if (nJoueurs <1) nJoueurs = 4; 
    }
    else if (!digitalRead(R0))
    {
      delay(200);
      nJoueurs ++;
      if (nJoueurs >4) nJoueurs = 1; 
    }
}
}
