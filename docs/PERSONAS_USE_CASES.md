# Documentation Personas & Use Cases - StemHub

Cette documentation détaille les profils utilisateurs cibles et les problématiques concrètes que StemHub résout à travers différents scénarios d'utilisation.

## 1. Tom Beats : Le Producteur de Musique
**Profil** : Jeune producteur de musique talentueux (22 ans, Paris).

### Bio
- **Activité** : Collabore régulièrement avec divers artistes.
- **Outils** : Utilise principalement FL Studio et Ableton.
- **Objectif** : Se faire connaître par les plus grands artistes.
- **Point de douleur** : Galère énormément dans sa gestion de fichiers (multiplication des versions "Save As").

### Use Case : Gestion des versions par Branches
**Le défi** : Un client demande une version "plus rapide avec une basse différente" tout en gardant l'originale. Tom crée un fichier `Projet_V2_Fast.flp`. Plus tard, il corrige le mixage du piano dans la V2 mais doit rouvrir la V1 pour refaire les mêmes réglages manuellement pour garder la cohérence.

**La solution StemHub** : Tom utilise le système de Branches.
1. Il crée une branche `feature-fast-tempo`.
2. Il effectue ses changements sur cette branche.
3. Lorsqu'il corrige le piano, il le fait sur la branche `main`.
4. Il Merge (fusionne) la correction vers la branche `feature-fast-tempo`.

**Résultat** : Les deux versions coexistent. Le piano est corrigé partout instantanément. Le client accède aux deux versions via un lien web unique sans export multiple.

---

## 2. Frank Bass : Le Bassiste Professionnel
**Profil** : Bassiste reconnu (40 ans, Marseille).

### Bio
- **Activité** : Enregistre pour de gros projets à distance.
- **Outils** : Enregistre ses instruments via Ableton.
- **Objectif** : Étendre son réseau professionnel.
- **Point de douleur** : Ses changements réguliers se perdent dans une forêt de fichiers `.zip` ; il oublie ses anciennes lignes de basse.

### Use Case : Récupération de pistes perdues
**User Story** : En tant que Frank Bass, je veux retrouver mes anciennes basslines perdues entre les fichiers `.zip` afin de ne plus perdre de temps dans mes archives désorganisées.

**Parcours Utilisateur** :
1. Frank réalise qu'il a perdu une bassline enregistrée il y a une semaine.
2. Il ouvre l'application StemHub et consulte l'historique visuel du projet.
3. Il sélectionne "Bassline Groove V2" datant de 7 jours et clique sur "Restaurer cette piste".
4. Ableton se met à jour automatiquement avec l'ancienne piste sans affecter le reste du projet actuel.

**Priorité (MoSCoW)** : **Must Have (M)**. C'est la fonctionnalité vitale pour le MVP pour résoudre le chaos des fichiers.

---

## 3. Mabé : Le Beatmaker Amateur
**Profil** : Beatmaker en chambre (23 ans, Chicago).

### Bio
- **Activité** : Produit pour se détendre après les cours, collabore via Discord.
- **Contraintes** : Espace de stockage limité (disque souvent saturé) sur ordinateur portable.
- **Point de douleur** : Peur de perdre 3 ans de projets si son disque tombe en panne. Écrase souvent ses bonnes idées par erreur en testant de nouveaux effets. Trouve WeTransfer trop lourd pour de simples "jams".

### Use Case : Restauration d'une piste spécifique
**User Story** : En tant que Mabé, je veux parcourir mes anciennes versions afin de retrouver une bassline supprimée par erreur.

**Parcours Utilisateur** :
1. Mabé regrette une basse supprimée deux jours auparavant.
2. Il consulte l'historique visuel dans l'application.
3. Il sélectionne "Bassline V1" et clique sur "Restaurer".
4. Le projet se synchronise et restaure la piste instantanément.

**Priorité (MoSCoW)** : **Must Have (M)**. Vital pour éviter la perte de données et le stress lié au stockage.

---

## 4. Laura : La Sound Designer
**Profil** : Sound Designer (29 ans, Paris).

### Bio
- **Problématique** : Travaille sur des projets complexes avec des milliers de fichiers.
- **Besoin** : Savoir exactement ce qui a changé entre deux versions (ex: volume d'une piste).
- **Motivation** : Le contrôle et la sécurité. Elle déteste ouvrir un projet lourd juste pour vérifier une modification.

### Use Case : Workflow de Contribution (Collaboration)
**Le défi** : Laura (à Bruxelles) doit poser des voix sur une prod venant de Londres (15 pistes à renvoyer en WAV brut).

**Le problème classique** : Devoir exporter 15 pistes une par une, vérifier le point de départ (0:00), créer un ZIP de 2 Go et attendre 1h d'upload sur WeTransfer. Confusion garantie à la moindre modification de phrase.

**La solution StemHub** :
1. Laura enregistre ses voix normalement dans son DAW.
2. Elle clique sur "Push". StemHub détecte intelligemment que seules les nouvelles pistes audio ont été ajoutées.
3. Le système n'envoie que ces données compressées sans perte.

**Résultat** : Le producteur à Londres reçoit une notification. Il clique sur "Pull". Les voix de Laura apparaissent instantanément dans sa session, parfaitement calées sur la grille. Aucun export manuel requis.
