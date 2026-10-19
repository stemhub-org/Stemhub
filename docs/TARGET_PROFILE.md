# StemHub — Profil cible & personas (v2)

> **Objectif obligatoire 1** — « Define your target ».
> **Remplace** les 4 personas de [PERSONAS_USE_CASES.md](./PERSONAS_USE_CASES.md) comme référence de ciblage. Les cas d'usage de ce fichier restent valables.
> **Statut :** ce sont des **proto-personas**. Ce sont des hypothèses construites à partir de ce que l'équipe connaît du terrain. Le guide Epitech précise qu'un persona écrit avant tout échange n'est qu'une hypothèse bien présentée. Chaque hypothèse est donc numérotée (H1…H10). Les entretiens du §7 la valident ou l'invalident, et c'est ce qui permet d'en faire un persona sourcé.

Documents liés : [ACQUISITION_STRATEGY.md](./ACQUISITION_STRATEGY.md) (où et comment aller chercher cette cible), [PARTNERSHIPS.md](./PARTNERSHIPS.md), [CREDIBILITY.md](./CREDIBILITY.md).

---

## 0. Pourquoi on resserre la cible

Les retours de la séance 3 :
- La cible était trop large. Il y avait 4 personas (beatmaker, bassiste de session sous Ableton, bedroom producer, sound designer) qui n'utilisent ni les mêmes logiciels ni les mêmes canaux.
- Un persona doit décrire le **quotidien pro et perso** de la personne. C'est ce qui permet de trouver les canaux pour l'atteindre.

Deux contraintes produit fixent les frontières :
1. **Le MVP ne fonctionne que sur FL Studio** (VST3, cf. [SPECIFICATION.md §2](./SPECIFICATION.md)). Un utilisateur d'Ableton ne peut pas tester le parcours principal.
2. **Ce qui nous différencie, c'est la collaboration, pas la sauvegarde.** La spec ([§5.3](./SPECIFICATION.md)) classe comme signal d'échec le cas où les testeurs n'utilisent StemHub que comme backup : on serait alors en concurrence frontale avec Drive et Dropbox.

La cible se trouve donc à l'intersection **FL Studio × produit régulièrement × collabore**.

---

## 1. La cible en une phrase

> Les **beatmakers et producteurs francophones** qui **produisent sur FL Studio au moins une fois par semaine** et qui **s'échangent des projets, des loops ou des stems avec au moins une autre personne** (autre producteur, artiste, ingé son).

Le cœur de cible est le **beatmaker rap / trap / drill / afro / pop urbaine**. C'est le genre où FL Studio domine et où la co-production entre beatmakers est une pratique courante (crédits « prod. X x Y »).

---

## 2. Critères vérifiables

Un prospect est **dans la cible** s'il remplit **C1, C2 et C3**. Les critères C4 à C7 servent à fixer la priorité et la faisabilité d'un test. Le CRM de prospection ([ACQUISITION_STRATEGY.md §6](./ACQUISITION_STRATEGY.md)) a une colonne par critère. Le guide Epitech précise en effet qu'un testeur qui ne remplit pas les critères ne compte pas.

| # | Critère | Seuil | Vérification avant contact | Vérification en entretien |
|---|---|---|---|---|
| **C1** | DAW principal = FL Studio | — | Bio (« FL Studio »), vidéos de cook-up, captures d'écran postées | « Sur quel logiciel tu produis ? » |
| **C2** | Produit régulièrement | ≥ 1 session par semaine **et** ≥ 4 prods publiées ou envoyées sur les 3 derniers mois | Dates de publication YouTube / BeatStars / SoundCloud / Instagram | « Combien de sessions par semaine ? » |
| **C3** | Collabore | ≥ 1 prod co-créée, envoyée ou reçue sur les 3 derniers mois | Crédits « prod. X x Y » ou « w/ @… » dans les titres, cook-ups à plusieurs | « Raconte ta dernière collab » |
| C4 | Échange des fichiers de projet | Partage de FLP zippés, de stems ou de loops, pas seulement des MP3 | — | « Quels fichiers vous vous envoyez ? » |
| C5 | Peut installer un VST3 | Ordinateur Windows ou macOS personnel | — | Question au formulaire |
| C6 | Francophone | — | Langue des posts | — |
| C7 | Majeur | ≥ 18 ans pour les entretiens et la beta | Déclaratif | Consentement à l'enregistrement |

**Justification de C6 et C7.** Pendant les entretiens et les tests UX en observation, on doit pouvoir mener des échanges longs et observer sans intervenir, donc en français. Les mineurs demandent un consentement parental pour l'enregistrement et le traitement des données (le service accepte les 15 ans et plus, cf. [SPECIFICATION.md §11.2](./SPECIFICATION.md), mais les tests sont réservés aux majeurs). Les producteurs anglophones forment un deuxième cercle, une fois le parcours validé en français.

---

## 3. Qui est explicitement exclu, et pourquoi

| Profil | Pourquoi il est exclu au MVP | Ce qu'on en fait |
|---|---|---|
| Utilisateurs **uniquement** d'Ableton, Logic, Pro Tools ou Studio One | Le plugin ne fonctionne que sur FL Studio. Ils ne peuvent pas tester le parcours principal. | Liste d'attente « autres DAW ». Le volume d'inscrits est un signal pour prioriser Ableton (stretch goal de la spec). |
| Producteurs **solo occasionnels** (< 1 session par mois, aucune collab) | Ils n'utiliseraient que la sauvegarde, ce qui nous met en concurrence avec Drive et Dropbox. Leurs retours tireraient le produit vers un outil de backup. | Pas de prospection active. |
| **Débutants** (< 6 mois de FL Studio) | Ils ne vivent pas encore le problème : peu de projets, pas de collab. Leurs retours portent sur l'apprentissage de FL Studio, pas sur StemHub. | Pas de prospection. Ils sont très visibles sur TikTok, ce qui crée un risque de « remplir » la communauté avec des non-cibles. |
| **Rappeurs, chanteurs, DJ** qui ne produisent pas | Ils n'utilisent pas le plugin. | Ce sont les **invités** du parcours collab (rôle secondaire, côté web). On les teste via les producteurs qui les invitent. |
| Utilisateurs **mobiles uniquement** (FL Studio Mobile, BandLab, GarageBand iOS) | Pas de VST3. | Hors cible. |
| **Studios pro / ingés son** sous Pro Tools | Autre DAW, autres besoins (recall de sessions), cycle de vente B2B. | Plus tard, éventuellement via l'offre Studio. |
| **Mineurs** | Consentement parental requis pour les entretiens et la beta. | Non sollicités. |
| Membres de l'équipe et **promo Epitech** de l'équipe | Règle Epitech : les testeurs doivent venir de l'extérieur. | Ne comptent ni dans les entretiens ni dans les testeurs. |

---

## 4. Proto-persona principal — « Yanis, le beatmaker collab »

| | |
|---|---|
| **Âge / situation** | 21 ans, grande ville (Paris, Lyon, Marseille, Lille…). En BTS ou en licence, ou en job alimentaire (livraison, vente, alternance). |
| **Setup** | FL Studio sur un PC portable Windows, casque + petites enceintes, clavier MIDI 25 touches. Plugins : Serum, Omnisphere, Gross Beat, Kontakt. Packs de samples (Splice, kits gratuits, kits de producteurs). |
| **Musique** | Trap / drill / afro. 4 à 6 ans de FL Studio. Publie des « type beats » sur YouTube et BeatStars. |
| **Revenus musique** (H) | Quelques licences vendues sur BeatStars, de rares placements chez des artistes locaux. Moins de 300 € par mois. |
| **Collabs** | Envoie des loops et des mélodies à d'autres beatmakers, qui ajoutent les drums, et en reçoit aussi. 1 à 3 co-prods par mois, créditées « prod. Yanis x … ». |

**Ce qu'il cherche à atteindre** : décrocher des placements chez des artistes connus, se constituer un réseau de producteurs, à terme vivre de la prod.

**Comment il fait aujourd'hui** (workflow de collab supposé) :
1. Il fait une loop dans FL Studio et l'exporte en WAV. Pour une vraie co-prod, il exporte le projet en `.zip` avec ses fichiers (*Export → Project data files*).
2. Il l'envoie par mail, DM Instagram, Discord, Google Drive ou WeTransfer.
3. Le co-producteur renvoie une version avec les drums (`beat_v3_final_FINAL.flp` ou un MP3).
4. Les allers-retours se font par messages vocaux. Les versions s'éparpillent entre les mails, le Drive et le dossier Téléchargements.
5. Quand un artiste valide la prod, il faut retrouver **la bonne version** pour exporter les pistes séparées (trackouts) destinées à l'ingé son.

**Ce qui le bloque** (hypothèses à valider) :
- **H1** — Il perd du temps, voire du travail, à retrouver la bonne version : fichiers `_v2_final`, backups automatiques de FL difficiles à exploiter.
- **H2** — Un FLP envoyé s'ouvre chez l'autre sans les samples, ou avec des plugins manquants.
- **H3** — Les crédits et les splits d'une co-prod reposent sur la confiance. Il n'existe aucune trace de qui a fait quoi, ni à quelle date.
- **H4** — Un crash de FL Studio ou un disque plein lui fait peur pour ses prods.
- **H5** — Le vocabulaire Git (commit, merge, pull request) le rebute. L'interface doit parler comme un producteur.

### Son quotidien pro et perso → où l'atteindre

C'est le lien direct avec les canaux d'acquisition ([ACQUISITION_STRATEGY.md](./ACQUISITION_STRATEGY.md)).

| Moment | Ce qu'il fait | Outils / lieux | Canal pour l'atteindre |
|---|---|---|---|
| **Journée** (cours, job, transports) | Scrolle pendant les pauses : reels de producteurs, cook-ups, breakdowns de prods | Instagram, TikTok, YouTube Shorts | DM et commentaires 1-to-1 sur Instagram et TikTok. Créateurs partenaires. |
| **Soirée / nuit** (21 h – 3 h) | Session FL Studio, souvent en vocal Discord avec d'autres beatmakers (cook-up en partage d'écran). Poste dans `#feedback` et `#collab`. | Discord, FL Studio | **Serveurs Discord de producteurs**, le cœur de l'outbound. |
| **Après une prod** | Upload YouTube (« type beat »), BeatStars, story Instagram. Envoie des loops à d'autres producteurs. | YouTube, BeatStars, mail | Mail pro en description YouTube ou BeatStars. Crédits « prod. X x Y » pour repérer les collaborateurs. |
| **Apprentissage** | Tutos FL Studio en français, threads d'entraide, packs gratuits | YouTube, Reddit r/FL_Studio, forums Audiofanzine | Posts « question » (pas de pub) sur les forums et Reddit. Partenariats avec des créateurs de tutos. |
| **Week-end** | Session studio avec un artiste (studio loué ou home studio), open mic, beat battle, concert | Studios, salles, événements | IRL : beat battles, studios et écoles partenaires ([PARTNERSHIPS.md](./PARTNERSHIPS.md)). |
| **Achats** | Plugins (pic au Black Friday), packs de samples, abonnements (BeatStars Pro, Splice, FL Cloud) | Sites d'éditeurs, Splice, BeatStars | Période du Black Friday, quand les producteurs pensent « outils » : moment propice pour leur parler d'un nouvel outil. |

**Qui l'influence** : les producteurs plus gros qu'il suit, ses co-producteurs et les modérateurs des serveurs où il est actif. Il fait peu confiance à la publicité, d'où l'inutilité d'un post Instagram sponsorisé.

### Ses moments émotionnels (pour la user journey et le referral)

| Moment | Émotion | Utilité pour StemHub |
|---|---|---|
| Crash de FL Studio, version écrasée, « c'était laquelle la bonne ? » | Panique, frustration | **Moment de besoin maximal.** L'accroche « Tu as déjà perdu une prod ? » parle à ce moment-là. |
| « Envoie-moi le FLP » → samples manquants | Agacement | Démo de *Save* / *Restore* : la version restaurée contient tous les fichiers audio et MIDI du projet. |
| Prod finie, vente, placement, crédit sur un son | Fierté, euphorie | **Moment pour demander un service** : témoignage, « invite ton co-prod ». |
| Restauration réussie d'une ancienne version | Soulagement | Deuxième moment de referral. Le témoignage est alors le plus sincère. |

**H8 — Le referral est intégré au produit.** Chaque collab oblige à inviter l'autre producteur. Si l'hypothèse se vérifie, chaque testeur actif fait entrer au moins un collaborateur.

---

## 5. Proto-persona secondaire — « Inès, la productrice qui travaille pour des artistes »

| | |
|---|---|
| **Âge / situation** | 26 ans, productrice et compositrice freelance (micro-entreprise), home studio. 8 ans de FL Studio. |
| **Réseau** | Travaille avec 3 à 5 artistes (rap, R&B, pop), un manager et un ingé son mix. Membre de la Sacem (H). |
| **Objectif** | Livrer vite et proprement, être créditée correctement, mener plusieurs projets en parallèle sans se perdre. |

**Comment elle fait aujourd'hui** : elle envoie des versions MP3 sur WhatsApp ou Drive. L'artiste répond par messages vocaux (« enlève la basse au refrain, remets la version d'avant »). Elle exporte les trackouts pour l'ingé son et les envoie par WeTransfer.

**Ce qui la bloque** (hypothèses) :
- **H6** — Retrouver la version que l'artiste a validée il y a trois semaines.
- **H7** — Les retours sont dispersés (vocaux WhatsApp, mails) et ne sont rattachés à aucune version précise.
- **H3 bis** — Prouver l'antériorité de sa prod et sa contribution en cas de litige.

**Son quotidien → où l'atteindre** :

| Moment | Outils / lieux | Canal |
|---|---|---|
| Journée en studio | WhatsApp, mail, Drive, WeTransfer | Recommandation par un pair (warm intro via un « Yanis » ou un studio) |
| Réseau pro | Instagram pro, LinkedIn | DM personnalisé sur Instagram ou LinkedIn |
| Événements | Rencontres pro (MaMA, ateliers Sacem et CNM), sessions studio | Rencontre IRL ([CREDIBILITY.md](./CREDIBILITY.md)) |
| Transmission | Ateliers ponctuels dans des écoles ou des MJC | Écoles partenaires ([PARTNERSHIPS.md](./PARTNERSHIPS.md)) |

**Rôle stratégique** : elle invite des artistes et des ingés dans l'app web, ce qui en fait une **prescriptrice**. C'est elle qui valide la valeur des commentaires et de la review par version. Elle est moins nombreuse que Yanis, mais chaque Inès fait entrer plusieurs personnes.

---

## 6. Anti-persona — « Lucas, 16 ans, débutant »

Lucas a découvert FL Studio grâce à TikTok il y a quatre mois. Il produit seul, ne publie pas et ne collabore pas. Son problème, c'est d'**apprendre FL Studio**, pas de versionner. Il ne remplit ni C2, ni C3, ni C7.

Pourquoi le nommer : il est **très nombreux et très visible** sur les réseaux, sous les mêmes hashtags que Yanis. Sans critères, il remplirait la communauté et fausserait les retours. Le nombre d'abonnés n'est pas un indicateur de réussite. Ce qui compte, c'est le nombre de membres qui remplissent C1 à C3.

---

## 7. Hypothèses à valider et comment les sourcer

**Moyen** : **20 entretiens de 15 à 20 minutes** (environ 15 profils « Yanis » et 5 profils « Inès »), recrutés selon [ACQUISITION_STRATEGY.md](./ACQUISITION_STRATEGY.md). Chaque entretien fait l'objet d'une fiche (§8). Les fiches servent de sources aux personas.

| # | Hypothèse | Seuil de validation (sur 20 entretiens) | Statut | Sources (n° de fiches) |
|---|---|---|---|---|
| H1 | Retrouver une version est un problème réel | ≥ 50 % racontent un épisode précis datant des 3 derniers mois | À valider | — |
| H2 | Les FLP partagés arrivent cassés (samples ou plugins manquants) | ≥ 40 % l'ont vécu | À valider | — |
| H3 | Les crédits et splits de co-prod posent problème | ≥ 30 % citent un conflit ou une incertitude | À valider | — |
| H4 | La peur de perdre ses prods (crash, disque) est réelle | ≥ 50 % ont déjà perdu du travail | À valider | — |
| H5 | Le vocabulaire Git rebute | Mots compris ou non lors d'un test rapide de libellés | À valider | — |
| H6 / H7 | (Inès) Les retours d'artistes sont dispersés et la version validée est difficile à retrouver | ≥ 3 « Inès » sur 5 | À valider | — |
| H8 | Le referral passe par l'invitation de collab | ≥ 1 invitation par testeur actif | Mesuré pendant la beta | — |
| H9 | Discord est le lieu principal d'échange entre producteurs | ≥ 60 % sont actifs sur au moins un serveur de prod | À valider | — |
| H10 | Une capacité à payer existe | ≥ 30 % paient déjà un abonnement musical (Splice, BeatStars Pro, FL Cloud…) | À valider | — |

**Signaux publics déjà observés** (ce ne sont pas des sources d'entretien, mais ils orientent les questions) :
- **H2** :
  - Sous [« How to Send FLPs to Other Producers for Collabs »](https://www.youtube.com/watch?v=qrBUkXK9-EI) (86 K vues), le commentaire le plus aimé demande comment éviter « sample not found » lors de l'envoi d'un FLP.
  - Sous [une autre vidéo](https://www.youtube.com/watch?v=cP5JKbuaoZg), des producteurs signalent des projets qui ne s'ouvrent pas d'une version de FL à l'autre, et des plugins manquants.
- **H3** : sous les vidéos sur l'envoi de loops, les questions sur les « terms of agreement » et le copyright des loops montrent que les splits restent flous.
- **H4** : FL Studio 2026 sauvegarde déjà les projets dans FL Cloud. Même si H4 se confirme, la sauvegarde seule ne différencie pas StemHub. C'est pour cette raison que le cœur de cible est défini par la **collaboration** (H2, H3, H6, H7).
- **H9** : les serveurs Discord francophones de beatmaking dépassent 10 000 membres (Producer Zone, RapZone). Des salons de cook-up et de recherche de collab existent ([ACQUISITION_STRATEGY.md §2](./ACQUISITION_STRATEGY.md)).

Une hypothèse invalidée n'est pas un échec. Elle **resserre** le persona, et ce qu'on a mesuré, appris et changé est précisément ce que le jury attend.

---

## 8. Guide d'entretien (15 à 20 minutes)

**Règles** : on pose des questions sur le passé, jamais sur le futur hypothétique (« tu utiliserais… ? »). On ne présente pas StemHub avant la fin de l'entretien. On demande le consentement avant d'enregistrer. Une personne pose les questions, une autre prend des notes en verbatim.

**Qualification**
1. Depuis combien de temps tu produis ? Sur quel logiciel ? Combien de sessions par semaine ? *(C1, C2)*
2. Raconte-moi ta dernière prod terminée, de l'idée jusqu'à l'envoi.

**Collaboration**

3. La dernière fois que vous avez travaillé à plusieurs sur un même son, comment ça s'est passé concrètement ? Quels fichiers vous êtes-vous envoyés, et par quel moyen ? *(C3, C4)*
4. Qu'est-ce qui a coincé ? *(relances : samples, plugins, versions, délais)* *(H2)*
5. Comment vous vous mettez d'accord sur les crédits et les splits ? *(H3)*

**Versions et sauvegarde**

6. Comment tu ranges tes projets ? *(en visio : « tu peux me montrer ton dossier ? »)*
7. La dernière fois que tu as voulu revenir à une ancienne version, qu'est-ce qui s'est passé ? *(H1, H6)*
8. Tu as déjà perdu une prod ? Raconte-moi. *(H4)*

**Solutions actuelles et valeur**

9. Qu'est-ce que tu utilises aujourd'hui pour ça ? Tu as déjà cherché autre chose ?
10. Qu'est-ce que tu paies aujourd'hui pour ta musique (plugins, Splice, BeatStars, FL Cloud…) ? *(H10)*

**Canaux** (alimente [ACQUISITION_STRATEGY.md](./ACQUISITION_STRATEGY.md))

11. Où est-ce que tu traînes en ligne pour parler prod ? Quels serveurs Discord, quelles chaînes, quels comptes tu suis ? *(H9)*
12. Où as-tu rencontré tes co-producteurs ?

**Clôture**

13. Qui d'autre je devrais interroger ? *(effet boule de neige, à tracer dans le CRM)*
14. Est-ce que je peux te recontacter pour tester une maquette ? *(vivier pour les tests UX)*

### Fiche d'entretien (une par entretien)

`N° · date · interviewer · canal de recrutement · pseudo / lien du profil · C1–C7 (✔/✘) · persona le plus proche · verbatims clés (citations exactes) · hypothèses confirmées / infirmées · canaux cités (Q11–12) · personnes recommandées (Q13) · OK test UX (oui/non)`

Ces fiches sont les **sources** exigées par Epitech pour les personas : un persona sans source ne compte pas.
